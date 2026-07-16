/// @file RuntimeAssetBootstrapTests.cpp
/// @brief Focused tests for the Runtime package asset bootstrap facade.

#include <SagaRuntime/RuntimeAssetBootstrap.hpp>

#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetManifestRegistryAdapter.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{

namespace Assets = SagaEngine::Assets;
namespace Resources = SagaEngine::Resources;
namespace Startup = SagaEngine::Startup;

using SagaRuntime::RuntimeAssetBootstrap;
using SagaRuntime::RuntimeAssetBootstrapDiagnostic;
using SagaRuntime::RuntimeAssetBootstrapDiagnosticCategory;
using SagaRuntime::RuntimeAssetBootstrapDiagnosticSeverity;
using SagaRuntime::RuntimeAssetBootstrapOptions;
using SagaRuntime::RuntimeAssetBootstrapResult;

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
    std::filesystem::create_directories(path);
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

[[nodiscard]] RuntimeAssetBootstrapOptions OptionsFor(
    const std::filesystem::path& packageManifestPath)
{
    RuntimeAssetBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.expectedRuntimeCompatibilityVersion = "0.0.9";
    options.validateReferencedManifestFiles = true;
    options.validateAssetFiles = true;
    return options;
}

[[nodiscard]] bool HasDiagnostic(
    const RuntimeAssetBootstrapResult& result,
    std::string_view diagnosticId)
{
    for (const RuntimeAssetBootstrapDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.diagnosticId == diagnosticId)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool HasCategory(
    const RuntimeAssetBootstrapResult& result,
    RuntimeAssetBootstrapDiagnosticCategory category)
{
    for (const RuntimeAssetBootstrapDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.category == category)
        {
            return true;
        }
    }

    return false;
}

void ExpectUsefulDiagnostic(const RuntimeAssetBootstrapResult& result)
{
    ASSERT_FALSE(result.diagnostics.empty());
    const RuntimeAssetBootstrapDiagnostic& diagnostic =
        result.diagnostics.front();
    EXPECT_EQ(diagnostic.severity, RuntimeAssetBootstrapDiagnosticSeverity::Error);
    EXPECT_FALSE(diagnostic.diagnosticId.empty());
    EXPECT_FALSE(diagnostic.message.empty());
}

} // namespace

TEST(RuntimeAssetBootstrapTests, ValidRuntimePackageRegistersOneAsset)
{
    Resources::AssetRegistry registry;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(FixturePackageManifestPath()));

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
}

TEST(RuntimeAssetBootstrapTests, MissingPackageManifestFailsWithoutMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_asset_bootstrap_missing_package");
    Resources::AssetRegistry registry;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(root / "missing_package_manifest.client.json"));

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_TRUE(HasDiagnostic(
        result,
        SagaEngine::Packages::PackageManifestDiagnostics::ManifestMissing));
    EXPECT_TRUE(HasCategory(
        result,
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetBootstrapTests, MissingIdentityManifestFailsWithoutMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_asset_bootstrap_missing_identity");
    CopyFixtureTo(root);
    std::filesystem::remove(root / "Manifests" / "asset_identity.json");

    Resources::AssetRegistry registry;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(root / "package_manifest.client.json"));

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_TRUE(HasDiagnostic(
        result,
        SagaEngine::Resources::AssetIdentityManifestDiagnostics::ManifestMissing));
    EXPECT_TRUE(HasCategory(
        result,
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetBootstrapTests, MissingCookedAssetFailsWhenValidationIsEnabled)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_asset_bootstrap_missing_cooked_asset");
    CopyFixtureTo(root);
    std::filesystem::remove(
        root / "Manifests" / "Cooked" / "runtime_smoke_texture.ktx2");

    Resources::AssetRegistry registry;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(root / "package_manifest.client.json"));

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_TRUE(HasDiagnostic(result, Assets::AssetManifestDiagnostics::FileMissing));
    EXPECT_TRUE(HasCategory(
        result,
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetBootstrapTests, MismatchedPackageBaseDirectoryFailsPredictably)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_asset_bootstrap_bad_base_dir");
    CopyFixtureTo(root);

    RuntimeAssetBootstrapOptions options =
        OptionsFor(root / "package_manifest.client.json");
    options.packageBaseDirectory = root / "WrongBase";

    Resources::AssetRegistry registry;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(registry, options);

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_FALSE(result.diagnostics.front().manifestPath.empty());
    EXPECT_NE(
        result.diagnostics.front().manifestPath.string().find("WrongBase"),
        std::string::npos);
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetBootstrapTests, BootstrapFailureDoesNotPartiallyMutateRegistry)
{
    Resources::AssetRegistry registry;
    Resources::AssetRegistryEntry existing;
    existing.assetId = kRuntimeSmokeAssetId;
    existing.assetKey = kRuntimeSmokeAssetKey;
    existing.kind = Resources::AssetKind::Texture;
    existing.sourcePath = "already_registered.ktx2";
    registry.Insert(std::move(existing));

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(FixturePackageManifestPath()));

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_TRUE(HasCategory(
        result,
        RuntimeAssetBootstrapDiagnosticCategory::AssetRegistryBootstrap));
    EXPECT_TRUE(HasDiagnostic(
        result,
        Resources::RuntimeAssetRegistryBootstrapDiagnostics::
            RegistryAssetKeyCollision));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 1u);
}

TEST(RuntimeAssetBootstrapTests, DomainMismatchUsesStartupGateDiagnostics)
{
    Resources::AssetRegistry registry;
    RuntimeAssetBootstrapOptions options = OptionsFor(FixturePackageManifestPath());
    options.expectedDomain = SagaRuntime::RuntimeStartupDomain::Server;

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(registry, options);

    ASSERT_FALSE(result.Succeeded());
    ExpectUsefulDiagnostic(result);
    EXPECT_TRUE(HasCategory(
        result,
        RuntimeAssetBootstrapDiagnosticCategory::StartupValidation));
    EXPECT_EQ(result.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}
