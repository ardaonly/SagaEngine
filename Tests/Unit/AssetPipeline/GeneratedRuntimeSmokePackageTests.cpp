/// @file GeneratedRuntimeSmokePackageTests.cpp
/// @brief Proves AssetPipeline writers can generate a RuntimeSmoke package.

#include <SagaAssetPipeline/Assets/AssetManifestWriter.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaAssetPipeline/Packages/PackageManifestWriter.hpp>
#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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

void WriteFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
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

[[nodiscard]] Pipeline::PackageManifestWriteInput RuntimeSmokePackageInput()
{
    Pipeline::PackageManifestWriteInput input;
    input.packageId = "saga.runtime_smoke.client";
    input.packageKind = Pipeline::PackageManifestKind::Client;
    input.buildProfile = "dev-client";
    input.targetPlatform = "linux";
    input.runtimeCompatibilityVersion = "0.0.9";
    input.assetIdentityManifest = "Manifests/asset_identity.json";
    input.assetManifests.push_back(Pipeline::PackageManifestRefWriteInput{
        .id = "assets.runtime_smoke",
        .path = "Manifests/assets.json"});
    return input;
}

void WriteGeneratedRuntimeSmokePackage(
    const std::filesystem::path& root,
    Pipeline::PackageManifestWriteInput packageInput = RuntimeSmokePackageInput())
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

    const Pipeline::PackageManifestWriteResult packageWriteResult =
        Pipeline::PackageManifestWriter::WriteToFile(
            PackageManifestPath(root),
            packageInput);
    ASSERT_TRUE(packageWriteResult.Succeeded());
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

TEST(GeneratedRuntimeSmokePackageTests,
     FullyGeneratedRuntimeSmokePackagePassesStartupAndRegistersAsset)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_package");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokePackage(root);

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_TRUE(startupResult.Succeeded());
    EXPECT_EQ(startupResult.packageManifest.packageId,
              "saga.runtime_smoke.client");
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

    const Resources::AssetRegistryEntry* byId =
        registry.Find(kRuntimeSmokeAssetId);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, kRuntimeSmokeAssetKey);
}

TEST(GeneratedRuntimeSmokePackageTests, MissingGeneratedPackageManifestFails)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_missing_package");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokePackage(root);
    std::filesystem::remove(PackageManifestPath(root));

    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(PackageManifestPath(root)));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Packages::PackageManifestDiagnostics::ManifestMissing));
}

TEST(GeneratedRuntimeSmokePackageTests, MissingGeneratedAssetManifestReferenceFails)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_missing_asset_ref");
    CopyFixtureTo(root);

    Pipeline::PackageManifestWriteInput input = RuntimeSmokePackageInput();
    input.assetManifests[0].path = "Manifests/missing_assets.json";
    WriteGeneratedRuntimeSmokePackage(root, input);

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Assets::AssetManifestDiagnostics::ManifestMissing));
}

TEST(GeneratedRuntimeSmokePackageTests,
     MissingGeneratedIdentityManifestReferenceFails)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_missing_identity_ref");
    CopyFixtureTo(root);

    Pipeline::PackageManifestWriteInput input = RuntimeSmokePackageInput();
    input.assetIdentityManifest = "Manifests/missing_asset_identity.json";
    WriteGeneratedRuntimeSmokePackage(root, input);

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Resources::AssetIdentityManifestDiagnostics::ManifestMissing));
}

TEST(GeneratedRuntimeSmokePackageTests,
     InvalidGeneratedAssetManifestFailsWithoutRegistryMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_generated_runtime_smoke_invalid_assets");
    CopyFixtureTo(root);
    WriteGeneratedRuntimeSmokePackage(root);
    WriteFile(AssetManifestPath(root), R"({ "schemaVersion": 1 })");

    const std::filesystem::path packageManifestPath = PackageManifestPath(root);
    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_FALSE(startupResult.Succeeded());
    EXPECT_TRUE(HasStartupDiagnostic(
        startupResult,
        Assets::AssetManifestDiagnostics::MissingField));

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
        Assets::AssetManifestDiagnostics::MissingField));
    EXPECT_EQ(bootstrapResult.registeredAssetCount, 0u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}
