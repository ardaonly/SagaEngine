/// @file GeneratedRuntimeSmokeManifestTests.cpp
/// @brief Proves production AssetPipeline writers can replace RuntimeSmoke manifests.

#include <SagaAssetPipeline/Assets/AssetManifestWriter.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

namespace
{

namespace Assets = SagaEngine::Assets;
namespace Packages = SagaEngine::Packages;
namespace Pipeline = SagaAssetPipeline;
namespace Resources = SagaEngine::Resources;
namespace Startup = SagaEngine::Startup;

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

[[nodiscard]] std::filesystem::path PackageManifestPath(
    const std::filesystem::path& root)
{
    return root / "package_manifest.client.json";
}

[[nodiscard]] std::filesystem::path AssetManifestPath(
    const std::filesystem::path& root)
{
    return root / "Manifests" / "assets.json";
}

[[nodiscard]] std::filesystem::path IdentityManifestPath(
    const std::filesystem::path& root)
{
    return root / "Manifests" / "asset_identity.json";
}

[[nodiscard]] Pipeline::AssetManifestWriterEntry RuntimeSmokeAssetEntry()
{
    Pipeline::AssetManifestWriterEntry entry;
    entry.id = kRuntimeSmokeAssetKey;
    entry.kind = "texture";
    entry.path = "Cooked/runtime_smoke_texture.ktx2";
    entry.hash = "fixture-runtime-smoke-texture";
    entry.memoryEstimateBytes = 16;
    entry.streamingGroup = "runtime-smoke";
    entry.platform = "linux";
    entry.profile = "dev-client";
    return entry;
}

void WriteGeneratedRuntimeSmokeManifests(const std::filesystem::path& root)
{
    const Pipeline::AssetIdentityManifestWriteResult identityWriteResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            IdentityManifestPath(root),
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = kRuntimeSmokeAssetKey,
                    .assetId = kRuntimeSmokeAssetId},
            });
    ASSERT_TRUE(identityWriteResult.Succeeded());

    const Pipeline::AssetManifestWriteResult assetWriteResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            AssetManifestPath(root),
            {
                RuntimeSmokeAssetEntry(),
            });
    ASSERT_TRUE(assetWriteResult.Succeeded());
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

[[nodiscard]] Packages::PackageManifest LoadPackageManifestForBootstrap(
    const std::filesystem::path& packageManifestPath)
{
    const Packages::PackageManifestLoadResult result =
        Packages::PackageManifestLoader::LoadFromFile(packageManifestPath);

    EXPECT_TRUE(result.Succeeded());
    return result.manifest;
}

[[nodiscard]] bool HasStartupDiagnostic(
    const Startup::RuntimeStartupGateResult& result,
    std::string_view diagnosticId)
{
    for (const Startup::RuntimeStartupGateDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.diagnosticId == diagnosticId)
        {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool HasBootstrapDiagnostic(
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

TEST(GeneratedRuntimeSmokeManifestTests,
     GeneratedManifestsReplaceRuntimeSmokeFixtureAndPassStartup)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_replacement");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokeManifests(root);

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_TRUE(startupResult.Succeeded());
    ASSERT_TRUE(startupResult.packageManifest.assetIdentityManifest.has_value());
    ASSERT_EQ(startupResult.packageManifest.assetManifests.size(), 1u);

    const Packages::PackageManifest packageManifest =
        LoadPackageManifestForBootstrap(packageManifestPath);
    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult bootstrapResult =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                packageManifest,
                BootstrapOptionsFor(packageManifestPath));

    ASSERT_TRUE(bootstrapResult.Succeeded());
    EXPECT_EQ(bootstrapResult.registeredAssetCount, 1u);
    EXPECT_EQ(registry.EntryCount(), 1u);

    const Resources::AssetRegistryEntry* byKey =
        registry.FindByKey(kRuntimeSmokeAssetKey);
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, kRuntimeSmokeAssetId);
    EXPECT_EQ(byKey->kind, Resources::AssetKind::Texture);
}

TEST(GeneratedRuntimeSmokeManifestTests,
     MissingGeneratedIdentityManifestFailsWithoutRegistryMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_missing_identity");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokeManifests(root);
    std::filesystem::remove(IdentityManifestPath(root));

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Resources::AssetIdentityManifestDiagnostics::ManifestMissing));

    const Packages::PackageManifest packageManifest =
        LoadPackageManifestForBootstrap(packageManifestPath);
    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult bootstrapResult =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                packageManifest,
                BootstrapOptionsFor(packageManifestPath));

    ASSERT_FALSE(bootstrapResult.Succeeded());
    EXPECT_TRUE(HasBootstrapDiagnostic(
        bootstrapResult,
        Resources::AssetIdentityManifestDiagnostics::ManifestMissing));
    EXPECT_EQ(bootstrapResult.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(GeneratedRuntimeSmokeManifestTests,
     MissingGeneratedAssetManifestFailsWithoutRegistryMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_missing_assets");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokeManifests(root);
    std::filesystem::remove(AssetManifestPath(root));

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Assets::AssetManifestDiagnostics::ManifestMissing));

    const Packages::PackageManifest packageManifest =
        LoadPackageManifestForBootstrap(packageManifestPath);
    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult bootstrapResult =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                packageManifest,
                BootstrapOptionsFor(packageManifestPath));

    ASSERT_FALSE(bootstrapResult.Succeeded());
    EXPECT_TRUE(HasBootstrapDiagnostic(
        bootstrapResult,
        Assets::AssetManifestDiagnostics::ManifestMissing));
    EXPECT_EQ(bootstrapResult.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}
