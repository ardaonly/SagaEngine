/// @file AssetManifestWriterTests.cpp
/// @brief Tests for Runtime-compatible AssetPipeline asset manifest writing.

#include <SagaAssetPipeline/Assets/AssetManifestWriter.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifest.hpp>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>

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

namespace Assets = SagaEngine::Assets;
namespace Packages = SagaEngine::Packages;
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

void WriteFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] bool HasDiagnostic(
    const Pipeline::AssetManifestWriteResult& result,
    std::string_view diagnosticId)
{
    for (const Pipeline::AssetManifestWriteDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.diagnosticId == diagnosticId)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] Pipeline::AssetManifestWriterEntry TextureEntry()
{
    Pipeline::AssetManifestWriterEntry entry;
    entry.id = "texture.runtime_smoke.albedo";
    entry.kind = "texture";
    entry.path = "Cooked/runtime_smoke_texture.ktx2";
    entry.hash = "sha256-runtime-smoke";
    entry.dependencies = {"material.runtime_smoke.hero"};
    entry.memoryEstimateBytes = 128;
    entry.streamingGroup = "runtime-smoke";
    entry.platform = "linux";
    entry.profile = "dev-client";
    return entry;
}

} // namespace

TEST(AssetManifestWriterTests, WritesRuntimeSchemaVersionAndAssetEntries)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_asset_manifest_writer_schema") /
        "Manifests" / "assets.json";

    const Pipeline::AssetManifestWriteResult writeResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            outputPath,
            {
                TextureEntry(),
            });

    ASSERT_TRUE(writeResult.Succeeded());
    EXPECT_EQ(writeResult.writtenAssetCount, 1u);

    const nlohmann::json root = ReadJson(outputPath);
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root.at("schemaVersion").get<int>(), 1);
    ASSERT_TRUE(root.at("assets").is_array());
    ASSERT_EQ(root.at("assets").size(), 1u);

    const nlohmann::json& asset = root.at("assets")[0];
    EXPECT_EQ(asset.at("id").get<std::string>(),
              "texture.runtime_smoke.albedo");
    EXPECT_EQ(asset.at("kind").get<std::string>(), "texture");
    EXPECT_EQ(asset.at("path").get<std::string>(),
              "Cooked/runtime_smoke_texture.ktx2");
    EXPECT_EQ(asset.at("hash").get<std::string>(), "sha256-runtime-smoke");
    ASSERT_TRUE(asset.at("dependencies").is_array());
    ASSERT_EQ(asset.at("dependencies").size(), 1u);
    EXPECT_EQ(asset.at("dependencies")[0].get<std::string>(),
              "material.runtime_smoke.hero");
    EXPECT_EQ(asset.at("memoryEstimateBytes").get<std::uint64_t>(), 128u);
    EXPECT_EQ(asset.at("streamingGroup").get<std::string>(), "runtime-smoke");
    EXPECT_EQ(asset.at("platform").get<std::string>(), "linux");
    EXPECT_EQ(asset.at("profile").get<std::string>(), "dev-client");
}

TEST(AssetManifestWriterTests, WriterOutputLoadsThroughRuntimeLoader)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_asset_manifest_writer_runtime_loader") / "assets.json";

    const Pipeline::AssetManifestWriteResult writeResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            outputPath,
            {
                TextureEntry(),
            });
    ASSERT_TRUE(writeResult.Succeeded());

    const Assets::AssetManifestLoadResult loadResult =
        Assets::AssetManifestLoader::LoadFromFile(outputPath);

    ASSERT_TRUE(loadResult.Succeeded());
    ASSERT_EQ(loadResult.manifest.assets.size(), 1u);
    EXPECT_EQ(loadResult.manifest.assets[0].id,
              "texture.runtime_smoke.albedo");
    EXPECT_EQ(loadResult.manifest.assets[0].kind, Assets::AssetKind::Texture);
    EXPECT_EQ(loadResult.manifest.assets[0].path,
              "Cooked/runtime_smoke_texture.ktx2");
    ASSERT_TRUE(loadResult.manifest.assets[0].memoryEstimateBytes.has_value());
    EXPECT_EQ(*loadResult.manifest.assets[0].memoryEstimateBytes, 128u);
}

TEST(AssetManifestWriterTests, WriterOutputPassesRuntimeFileValidation)
{
    const std::filesystem::path root =
        TempRoot("saga_asset_manifest_writer_file_validation");
    const std::filesystem::path assetPath =
        root / "Manifests" / "Cooked" / "runtime_smoke_texture.ktx2";
    WriteFile(assetPath, "texture");

    const std::filesystem::path outputPath = root / "Manifests" / "assets.json";
    const Pipeline::AssetManifestWriteResult writeResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            outputPath,
            {
                TextureEntry(),
            });
    ASSERT_TRUE(writeResult.Succeeded());

    Assets::AssetManifestLoadOptions options;
    options.validateAssetFiles = true;
    options.assetBaseDirectory = outputPath.parent_path();

    const Assets::AssetManifestLoadResult loadResult =
        Assets::AssetManifestLoader::LoadFromFile(outputPath, options);

    ASSERT_TRUE(loadResult.Succeeded());
    ASSERT_EQ(loadResult.manifest.assets.size(), 1u);
    EXPECT_EQ(loadResult.manifest.assets[0].id,
              "texture.runtime_smoke.albedo");
}

TEST(AssetManifestWriterTests, GeneratedAssetAndIdentityManifestsBootstrapRegistry)
{
    const std::filesystem::path root =
        TempRoot("saga_asset_manifest_writer_bootstrap");
    const std::filesystem::path manifestRoot = root / "Manifests";
    WriteFile(manifestRoot / "Cooked" / "runtime_smoke_texture.ktx2", "texture");

    const Pipeline::AssetIdentityManifestWriteResult identityWriteResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            manifestRoot / "asset_identity.json",
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.runtime_smoke.albedo",
                    .assetId = 5001},
            });
    ASSERT_TRUE(identityWriteResult.Succeeded());

    const Pipeline::AssetManifestWriteResult assetWriteResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            manifestRoot / "assets.json",
            {
                TextureEntry(),
            });
    ASSERT_TRUE(assetWriteResult.Succeeded());

    Packages::PackageManifest packageManifest;
    packageManifest.schemaVersion = 1;
    packageManifest.packageId = "saga.asset_manifest_writer.client";
    packageManifest.packageKind = Packages::PackageKind::Client;
    packageManifest.buildProfile = "dev-client";
    packageManifest.targetPlatform = "linux";
    packageManifest.runtimeCompatibilityVersion = "0.0.9";
    packageManifest.assetIdentityManifest = "Manifests/asset_identity.json";
    packageManifest.assetManifests.push_back(Packages::PackageManifestRef{
        .id = "runtime-smoke-assets",
        .path = "Manifests/assets.json"});

    Resources::RuntimeAssetRegistryBootstrapOptions options;
    options.packageManifestPath = root / "package_manifest.client.json";
    options.validateAssetFiles = true;

    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult result =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                packageManifest,
                options);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    EXPECT_EQ(registry.EntryCount(), 1u);

    const Resources::AssetRegistryEntry* entry =
        registry.FindByKey("texture.runtime_smoke.albedo");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->assetId, 5001u);
    EXPECT_EQ(entry->kind, Resources::AssetKind::Texture);
}

TEST(AssetManifestWriterTests, RejectsInvalidAssetsWithoutWritingFile)
{
    struct Case
    {
        const char* name;
        std::vector<Pipeline::AssetManifestWriterEntry> assets;
        const char* diagnosticId;
    };

    Pipeline::AssetManifestWriterEntry emptyId = TextureEntry();
    emptyId.id.clear();

    Pipeline::AssetManifestWriterEntry unknownKind = TextureEntry();
    unknownKind.kind = "document";

    Pipeline::AssetManifestWriterEntry escapingPath = TextureEntry();
    escapingPath.path = "../Cooked/runtime_smoke_texture.ktx2";

    Pipeline::AssetManifestWriterEntry duplicateA = TextureEntry();
    Pipeline::AssetManifestWriterEntry duplicateB = TextureEntry();
    duplicateB.path = "Cooked/runtime_smoke_texture_copy.ktx2";

    Pipeline::AssetManifestWriterEntry invalidDependency = TextureEntry();
    invalidDependency.dependencies = {""};

    const std::vector<Case> cases = {
        Case{
            .name = "empty_id",
            .assets = {emptyId},
            .diagnosticId =
                Pipeline::AssetManifestWriterDiagnostics::InvalidAssetId,
        },
        Case{
            .name = "unknown_kind",
            .assets = {unknownKind},
            .diagnosticId =
                Pipeline::AssetManifestWriterDiagnostics::InvalidAssetKind,
        },
        Case{
            .name = "escaping_path",
            .assets = {escapingPath},
            .diagnosticId =
                Pipeline::AssetManifestWriterDiagnostics::InvalidAssetPath,
        },
        Case{
            .name = "duplicate_id",
            .assets = {duplicateA, duplicateB},
            .diagnosticId =
                Pipeline::AssetManifestWriterDiagnostics::DuplicateAssetId,
        },
        Case{
            .name = "invalid_dependency",
            .assets = {invalidDependency},
            .diagnosticId =
                Pipeline::AssetManifestWriterDiagnostics::InvalidDependency,
        },
    };

    for (const Case& testCase : cases)
    {
        const std::string tempName =
            std::string("saga_asset_manifest_writer_") + testCase.name;
        const std::filesystem::path outputPath =
            TempRoot(tempName.c_str()) / "assets.json";

        const Pipeline::AssetManifestWriteResult writeResult =
            Pipeline::AssetManifestWriter::WriteToFile(
                outputPath,
                testCase.assets);

        EXPECT_FALSE(writeResult.Succeeded()) << testCase.name;
        EXPECT_TRUE(HasDiagnostic(writeResult, testCase.diagnosticId))
            << testCase.name;
        EXPECT_FALSE(std::filesystem::exists(outputPath)) << testCase.name;
    }
}
