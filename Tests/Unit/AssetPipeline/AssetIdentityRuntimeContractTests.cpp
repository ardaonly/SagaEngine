/// @file AssetIdentityRuntimeContractTests.cpp
/// @brief Contract tests from AssetPipeline identity generation to Runtime loading.

#include <SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace Pipeline = SagaAssetPipeline;
namespace Packages = SagaEngine::Packages;
namespace Resources = SagaEngine::Resources;

constexpr const char* kRuntimeSmokeAssetKey = "texture.runtime_smoke.albedo";
constexpr Resources::AssetId kRuntimeSmokeAssetId = 5001;

[[nodiscard]] std::filesystem::path FixtureRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" / "Fixtures" /
           "Packages" / "RuntimeSmoke";
}

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    return path;
}

void CopyFixtureTo(const std::filesystem::path& destination)
{
    std::filesystem::copy(
        FixtureRoot(),
        destination,
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
}

void WriteIdentityManifest(
    const std::filesystem::path& path,
    const std::vector<Pipeline::AssetIdentityMapping>& mappings)
{
    nlohmann::json root = {
        {"schemaVersion", 1},
        {"mappings", nlohmann::json::array()},
    };

    for (const Pipeline::AssetIdentityMapping& mapping : mappings)
    {
        root["mappings"].push_back({
            {"assetKey", mapping.assetKey},
            {"assetId", mapping.assetId},
        });
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << root.dump(2) << '\n';
}

[[nodiscard]] Resources::AssetIdentityManifestLoadResult
LoadGeneratedIdentityManifest(
    const char* tempName,
    const std::vector<Pipeline::AssetIdentityMapping>& mappings)
{
    const std::filesystem::path manifestPath =
        TempRoot(tempName) / "asset_identity.json";
    WriteIdentityManifest(manifestPath, mappings);
    return Resources::AssetIdentityManifestLoader::LoadFromFile(manifestPath);
}

[[nodiscard]] std::optional<Pipeline::AssetId> FindAssetId(
    const std::vector<Pipeline::AssetIdentityMapping>& mappings,
    std::string_view assetKey)
{
    for (const Pipeline::AssetIdentityMapping& mapping : mappings)
    {
        if (mapping.assetKey == assetKey)
        {
            return mapping.assetId;
        }
    }

    return std::nullopt;
}

[[nodiscard]] Packages::PackageManifest LoadPackageManifest(
    const std::filesystem::path& packageManifestPath)
{
    Packages::PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;

    const Packages::PackageManifestLoadResult result =
        Packages::PackageManifestLoader::LoadFromFile(
            packageManifestPath,
            options);

    EXPECT_TRUE(result.Succeeded());
    return result.manifest;
}

[[nodiscard]] Resources::RuntimeAssetRegistryBootstrapOptions BootstrapOptionsFor(
    const std::filesystem::path& packageManifestPath)
{
    Resources::RuntimeAssetRegistryBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.validateAssetFiles = true;
    return options;
}

} // namespace

TEST(AssetIdentityRuntimeContractTests, GeneratedMappingsLoadThroughRuntimeSchema)
{
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = "material.hero"},
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.hero.diffuse"},
            },
            {});

    ASSERT_TRUE(generated.Succeeded());

    const Resources::AssetIdentityManifestLoadResult loaded =
        LoadGeneratedIdentityManifest(
            "saga_asset_identity_contract_schema",
            generated.mappings);

    ASSERT_TRUE(loaded.Succeeded());
    EXPECT_EQ(loaded.manifest.schemaVersion, 1u);
    ASSERT_EQ(loaded.manifest.mappings.size(), 2u);
    EXPECT_EQ(loaded.manifest.mappings[0].assetKey, "material.hero");
    EXPECT_EQ(loaded.manifest.mappings[0].assetId, 1u);
    EXPECT_EQ(loaded.manifest.mappings[1].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(loaded.manifest.mappings[1].assetId, 2u);
}

TEST(AssetIdentityRuntimeContractTests, PreviousIdsSurviveRuntimeResolverLoad)
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
    EXPECT_EQ(FindAssetId(generated.mappings, "material.hero"), 9001u);

    const Resources::AssetIdentityManifestLoadResult loaded =
        LoadGeneratedIdentityManifest(
            "saga_asset_identity_contract_previous_ids",
            generated.mappings);
    ASSERT_TRUE(loaded.Succeeded());

    const Resources::StaticAssetIdResolver resolver =
        Resources::AssetIdentityManifestLoader::BuildResolver(loaded.manifest);
    Resources::AssetId assetId = Resources::kInvalidAssetId;
    ASSERT_TRUE(resolver.ResolveAssetId("material.hero", assetId));
    EXPECT_EQ(assetId, 9001u);
}

TEST(AssetIdentityRuntimeContractTests, NewIdsAllocateDeterministicallyAndLoad)
{
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.zeta"},
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.alpha"},
            },
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.deleted",
                    .assetId = 40},
            });

    ASSERT_TRUE(generated.Succeeded());
    EXPECT_EQ(FindAssetId(generated.mappings, "texture.alpha"), 41u);
    EXPECT_EQ(FindAssetId(generated.mappings, "texture.zeta"), 42u);

    const Resources::AssetIdentityManifestLoadResult loaded =
        LoadGeneratedIdentityManifest(
            "saga_asset_identity_contract_deterministic_ids",
            generated.mappings);

    ASSERT_TRUE(loaded.Succeeded());
    ASSERT_EQ(loaded.manifest.mappings.size(), 2u);
    EXPECT_EQ(loaded.manifest.mappings[0].assetKey, "texture.alpha");
    EXPECT_EQ(loaded.manifest.mappings[0].assetId, 41u);
    EXPECT_EQ(loaded.manifest.mappings[1].assetKey, "texture.zeta");
    EXPECT_EQ(loaded.manifest.mappings[1].assetId, 42u);
}

TEST(AssetIdentityRuntimeContractTests, GeneratedManifestBootstrapsRuntimeSmokeFixture)
{
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = kRuntimeSmokeAssetKey},
            },
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = kRuntimeSmokeAssetKey,
                    .assetId = kRuntimeSmokeAssetId},
            });

    ASSERT_TRUE(generated.Succeeded());

    const std::filesystem::path root =
        TempRoot("saga_asset_identity_contract_runtime_smoke");
    CopyFixtureTo(root);
    WriteIdentityManifest(
        root / "Manifests" / "asset_identity.json",
        generated.mappings);

    const std::filesystem::path packageManifestPath =
        root / "package_manifest.client.json";
    const Packages::PackageManifest packageManifest =
        LoadPackageManifest(packageManifestPath);
    Resources::AssetRegistry registry;

    const Resources::RuntimeAssetRegistryBootstrapResult result =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                packageManifest,
                BootstrapOptionsFor(packageManifestPath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    EXPECT_EQ(registry.EntryCount(), 1u);

    const Resources::AssetRegistryEntry* entry =
        registry.FindByKey(kRuntimeSmokeAssetKey);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->assetId, kRuntimeSmokeAssetId);
    EXPECT_EQ(entry->kind, Resources::AssetKind::Texture);
}

TEST(AssetIdentityRuntimeContractTests, DuplicateInputKeysFailBeforeSerialization)
{
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
    ASSERT_EQ(generated.errors.size(), 1u);
    EXPECT_EQ(
        generated.errors[0].diagnosticId,
        Pipeline::AssetIdentityGenerationDiagnostics::DuplicateInputAssetKey);
    EXPECT_TRUE(generated.mappings.empty());
}

TEST(AssetIdentityRuntimeContractTests, InvalidPreviousAssetIdFailsBeforeSerialization)
{
    const Pipeline::AssetIdentityGenerationResult generated =
        Pipeline::AssetIdentityGenerator::Generate(
            {
                Pipeline::AssetIdentityInput{
                    .assetKey = "texture.hero.diffuse"},
            },
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero.diffuse",
                    .assetId = Pipeline::kInvalidAssetId},
            });

    ASSERT_FALSE(generated.Succeeded());
    ASSERT_EQ(generated.errors.size(), 1u);
    EXPECT_EQ(
        generated.errors[0].diagnosticId,
        Pipeline::AssetIdentityGenerationDiagnostics::InvalidAssetId);
    EXPECT_TRUE(generated.mappings.empty());
}

TEST(AssetIdentityRuntimeContractTests, DuplicatePreviousIdsFailBeforeSerialization)
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
                    .assetId = 1001},
                Pipeline::AssetIdentityMapping{
                    .assetKey = "texture.hero.diffuse",
                    .assetId = 1001},
            });

    ASSERT_FALSE(generated.Succeeded());
    ASSERT_EQ(generated.errors.size(), 1u);
    EXPECT_EQ(
        generated.errors[0].diagnosticId,
        Pipeline::AssetIdentityGenerationDiagnostics::DuplicateAssetId);
    EXPECT_TRUE(generated.mappings.empty());
}
