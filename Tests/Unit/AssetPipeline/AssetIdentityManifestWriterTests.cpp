/// @file AssetIdentityManifestWriterTests.cpp
/// @brief Tests for Runtime-compatible AssetPipeline identity manifest writing.

#include <SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace Pipeline = SagaAssetPipeline;
namespace Resources = SagaEngine::Resources;

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    return path;
}

[[nodiscard]] nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    nlohmann::json root;
    input >> root;
    return root;
}

[[nodiscard]] bool HasDiagnostic(
    const Pipeline::AssetIdentityManifestWriteResult& result,
    std::string_view diagnosticId)
{
    for (const Pipeline::AssetIdentityManifestWriteDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.diagnosticId == diagnosticId)
        {
            return true;
        }
    }

    return false;
}

} // namespace

TEST(AssetIdentityManifestWriterTests, WritesRuntimeSchemaVersionAndMappings)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_asset_identity_writer_schema") /
        "Manifests" / "asset_identity.json";

    const Pipeline::AssetIdentityManifestWriteResult writeResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            outputPath,
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "material.hero",
                    .assetId = 1001},
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero.diffuse",
                    .assetId = 1002},
            });

    ASSERT_TRUE(writeResult.Succeeded());
    EXPECT_EQ(writeResult.writtenMappingCount, 2u);

    const nlohmann::json root = ReadJson(outputPath);
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root.at("schemaVersion").get<int>(), 1);
    ASSERT_TRUE(root.at("mappings").is_array());
    ASSERT_EQ(root.at("mappings").size(), 2u);
    EXPECT_EQ(root.at("mappings")[0].at("assetKey").get<std::string>(),
              "material.hero");
    EXPECT_EQ(root.at("mappings")[0].at("assetId").get<std::uint64_t>(),
              1001u);
    EXPECT_EQ(root.at("mappings")[1].at("assetKey").get<std::string>(),
              "texture.hero.diffuse");
    EXPECT_EQ(root.at("mappings")[1].at("assetId").get<std::uint64_t>(),
              1002u);
}

TEST(AssetIdentityManifestWriterTests, WriterOutputLoadsThroughRuntimeLoader)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_asset_identity_writer_runtime_loader") /
        "asset_identity.json";

    const Pipeline::AssetIdentityManifestWriteResult writeResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            outputPath,
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero.diffuse",
                    .assetId = 42},
            });
    ASSERT_TRUE(writeResult.Succeeded());

    const Resources::AssetIdentityManifestLoadResult loadResult =
        Resources::AssetIdentityManifestLoader::LoadFromFile(outputPath);

    ASSERT_TRUE(loadResult.Succeeded());
    ASSERT_EQ(loadResult.manifest.mappings.size(), 1u);
    EXPECT_EQ(loadResult.manifest.mappings[0].assetKey,
              "texture.hero.diffuse");
    EXPECT_EQ(loadResult.manifest.mappings[0].assetId, 42u);
}

TEST(AssetIdentityManifestWriterTests, StableIdsSurviveGeneratorWriteReadResolver)
{
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = "material.hero"},
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.hero.diffuse"},
            },
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "material.hero",
                    .assetId = 9001},
            });
    ASSERT_TRUE(generated.Succeeded());

    const std::filesystem::path outputPath =
        TempRoot("saga_asset_identity_writer_generator_roundtrip") /
        "asset_identity.json";
    const Pipeline::AssetIdentityManifestWriteResult writeResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            outputPath,
            generated.mappings);
    ASSERT_TRUE(writeResult.Succeeded());

    const Resources::AssetIdentityManifestLoadResult loadResult =
        Resources::AssetIdentityManifestLoader::LoadFromFile(outputPath);
    ASSERT_TRUE(loadResult.Succeeded());

    const Resources::StaticAssetIdResolver resolver =
        Resources::AssetIdentityManifestLoader::BuildResolver(
            loadResult.manifest);
    Resources::AssetId assetId = Resources::kInvalidAssetId;
    ASSERT_TRUE(resolver.ResolveAssetId("material.hero", assetId));
    EXPECT_EQ(assetId, 9001u);
}

TEST(AssetIdentityManifestWriterTests, InvalidGeneratorInputsDoNotProduceArtifact)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_asset_identity_writer_invalid_generator") /
        "asset_identity.json";
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.hero.diffuse"},
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.hero.diffuse"},
            },
            {});

    ASSERT_FALSE(generated.Succeeded());
    EXPECT_TRUE(generated.mappings.empty());
    EXPECT_FALSE(std::filesystem::exists(outputPath));
}

TEST(AssetIdentityManifestWriterTests, RejectsInvalidMappingsWithoutWritingFile)
{
    struct Case
    {
        const char* name;
        std::vector<Pipeline::AssetIdentityMapping> mappings;
        const char* diagnosticId;
    };

    const std::vector<Case> cases = {
        Case{
            .name = "empty_key",
            .mappings = {
                Pipeline::AssetIdentityMapping{.assetKey = "", .assetId = 1},
            },
            .diagnosticId =
                Pipeline::AssetIdentityManifestWriterDiagnostics::InvalidAssetKey,
        },
        Case{
            .name = "zero_id",
            .mappings = {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero",
                    .assetId = Pipeline::kInvalidAssetId},
            },
            .diagnosticId =
                Pipeline::AssetIdentityManifestWriterDiagnostics::InvalidAssetId,
        },
        Case{
            .name = "duplicate_key",
            .mappings = {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero",
                    .assetId = 1},
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero",
                    .assetId = 2},
            },
            .diagnosticId =
                Pipeline::AssetIdentityManifestWriterDiagnostics::DuplicateAssetKey,
        },
        Case{
            .name = "duplicate_id",
            .mappings = {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero",
                    .assetId = 1},
                Pipeline::AssetIdentityMapping{
                    .assetKey = "material.hero",
                    .assetId = 1},
            },
            .diagnosticId =
                Pipeline::AssetIdentityManifestWriterDiagnostics::DuplicateAssetId,
        },
    };

    for (const Case& testCase : cases)
    {
        const std::filesystem::path outputPath =
            TempRoot(testCase.name) / "asset_identity.json";

        const Pipeline::AssetIdentityManifestWriteResult writeResult =
            Pipeline::AssetIdentityManifestWriter::WriteToFile(
                outputPath,
                testCase.mappings);

        EXPECT_FALSE(writeResult.Succeeded()) << testCase.name;
        EXPECT_TRUE(HasDiagnostic(writeResult, testCase.diagnosticId))
            << testCase.name;
        EXPECT_FALSE(std::filesystem::exists(outputPath)) << testCase.name;
    }
}
