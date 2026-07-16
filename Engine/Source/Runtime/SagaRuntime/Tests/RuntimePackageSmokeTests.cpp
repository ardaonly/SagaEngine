/// @file RuntimePackageSmokeTests.cpp
/// @brief Focused smoke tests for checked-in runtime package fixtures.

#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetManifestRegistryAdapter.h>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{

namespace Assets = SagaEngine::Assets;
namespace Packages = SagaEngine::Packages;
namespace Resources = SagaEngine::Resources;
namespace Startup = SagaEngine::Startup;

constexpr const char* kRuntimeSmokeAssetKey = "texture.runtime_smoke.albedo";
constexpr Resources::AssetId kRuntimeSmokeAssetId = 5001;

[[nodiscard]] std::filesystem::path FixtureRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" / "Fixtures" /
           "Packages" / "RuntimeSmoke";
}

[[nodiscard]] std::filesystem::path FixturePackageManifestPath()
{
    return FixtureRoot() / "package_manifest.client.json";
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

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] Startup::RuntimeStartupGateOptions StartupOptionsFor(
    const std::filesystem::path& packageManifestPath)
{
    Startup::RuntimeStartupGateOptions options;
    options.packageManifestPath = packageManifestPath;
    options.expectedDomain = Startup::RuntimeStartupDomain::Client;
    options.expectedRuntimeCompatibilityVersion = "0.0.9";
    options.validateReferencedManifestFiles = true;
    options.validateAssetFiles = true;
    return options;
}

[[nodiscard]] Resources::RuntimeAssetRegistryBootstrapOptions BootstrapOptionsFor(
    const std::filesystem::path& packageManifestPath)
{
    Resources::RuntimeAssetRegistryBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.validateAssetFiles = true;
    return options;
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

[[nodiscard]] bool HasDiagnostic(
    const Resources::RuntimeAssetRegistryBootstrapResult& result,
    std::string_view diagnosticId)
{
    for (const Resources::RuntimeAssetRegistryBootstrapDiagnostic& diagnostic :
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

TEST(RuntimePackageSmokeTests, FixturePackagePassesStartupGate)
{
    const Startup::RuntimeStartupGateResult result =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(FixturePackageManifestPath()));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.packageManifest.packageId, "saga.runtime_smoke.client");
    ASSERT_EQ(result.packageManifest.assetManifests.size(), 1u);
    ASSERT_TRUE(result.packageManifest.assetIdentityManifest.has_value());
}

TEST(RuntimePackageSmokeTests, FixtureRegistersSingleAsset)
{
    const std::filesystem::path packageManifestPath =
        FixturePackageManifestPath();
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

    const Resources::AssetRegistryEntry* byKey =
        registry.FindByKey(kRuntimeSmokeAssetKey);
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, kRuntimeSmokeAssetId);
    EXPECT_EQ(byKey->kind, Resources::AssetKind::Texture);

    const Resources::AssetRegistryEntry* byId =
        registry.Find(kRuntimeSmokeAssetId);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, kRuntimeSmokeAssetKey);
    EXPECT_EQ(
        std::filesystem::path(byId->sourcePath).lexically_normal(),
        (FixtureRoot() / "Manifests" / "Cooked" /
         "runtime_smoke_texture.ktx2")
            .lexically_normal());
}

TEST(RuntimePackageSmokeTests, MissingIdentityMappingFailsWithoutRegistryMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_package_smoke_missing_identity_mapping");
    CopyFixtureTo(root);
    WriteFile(
        root / "Manifests" / "asset_identity.json",
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.unrelated", "assetId": 5002 }
  ]
})");

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

    ASSERT_FALSE(result.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        result,
        Resources::AssetManifestRegistryAdapterDiagnostics::
            MissingAssetIdMapping));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimePackageSmokeTests, MissingCookedFileFailsWhenAssetValidationIsEnabled)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_package_smoke_missing_cooked_asset");
    CopyFixtureTo(root);
    std::filesystem::remove(
        root / "Manifests" / "Cooked" / "runtime_smoke_texture.ktx2");

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

    ASSERT_FALSE(result.Succeeded());
    EXPECT_TRUE(HasDiagnostic(
        result,
        Assets::AssetManifestDiagnostics::FileMissing));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}
