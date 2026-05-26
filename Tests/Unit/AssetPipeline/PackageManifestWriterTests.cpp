/// @file PackageManifestWriterTests.cpp
/// @brief Tests for Runtime-compatible AssetPipeline package manifest writing.

#include <SagaAssetPipeline/Assets/AssetManifestWriter.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaAssetPipeline/Packages/PackageManifestWriter.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace Packages = SagaEngine::Packages;
namespace Pipeline = SagaAssetPipeline;
namespace Resources = SagaEngine::Resources;
namespace Startup = SagaEngine::Startup;

constexpr const char* kRuntimeSmokeAssetKey = "texture.runtime_smoke.albedo";
constexpr Resources::AssetId kRuntimeSmokeAssetId = 5001;

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    return path;
}

[[nodiscard]] std::filesystem::path FixtureRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" / "Fixtures" /
           "Packages" / "RuntimeSmoke";
}

[[nodiscard]] nlohmann::json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path);
    nlohmann::json root;
    input >> root;
    return root;
}

[[nodiscard]] bool HasDiagnostic(
    const Pipeline::PackageManifestWriteResult& result,
    std::string_view diagnosticId)
{
    for (const Pipeline::PackageManifestWriteDiagnostic& diagnostic :
         result.diagnostics)
    {
        if (diagnostic.diagnosticId == diagnosticId)
        {
            return true;
        }
    }

    return false;
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
        .path = "Manifests/assets.json",
        .hash = "assets-hash"});
    input.packageHash = "package-hash";
    return input;
}

void WriteGeneratedRuntimeSmokePayload(const std::filesystem::path& root)
{
    const std::filesystem::path cookedPath =
        root / "Manifests" / "Cooked" / "runtime_smoke_texture.ktx2";
    std::filesystem::create_directories(cookedPath.parent_path());
    std::filesystem::copy_file(
        FixtureRoot() / "Manifests" / "Cooked" /
            "runtime_smoke_texture.ktx2",
        cookedPath,
        std::filesystem::copy_options::overwrite_existing);

    const Pipeline::AssetIdentityManifestWriteResult identityWriteResult =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            root / "Manifests" / "asset_identity.json",
            {
                Pipeline::AssetIdentityMapping{
                    .assetKey = kRuntimeSmokeAssetKey,
                    .assetId = kRuntimeSmokeAssetId},
            });
    ASSERT_TRUE(identityWriteResult.Succeeded());

    const Pipeline::AssetManifestWriteResult assetWriteResult =
        Pipeline::AssetManifestWriter::WriteToFile(
            root / "Manifests" / "assets.json",
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

} // namespace

TEST(PackageManifestWriterTests, WritesRuntimeCompatiblePackageManifestJson)
{
    const std::filesystem::path outputPath =
        TempRoot("saga_package_manifest_writer_schema") /
        "package_manifest.client.json";

    Pipeline::PackageManifestWriteInput input = RuntimeSmokePackageInput();
    input.artifactManifests.push_back(Pipeline::PackageManifestRefWriteInput{
        .id = "artifacts.runtime_smoke",
        .path = "Manifests/artifacts.json",
        .hash = "artifacts-hash"});

    const Pipeline::PackageManifestWriteResult writeResult =
        Pipeline::PackageManifestWriter::WriteToFile(outputPath, input);

    ASSERT_TRUE(writeResult.Succeeded());
    EXPECT_EQ(writeResult.writtenAssetManifestCount, 1u);
    EXPECT_EQ(writeResult.writtenArtifactManifestCount, 1u);

    const nlohmann::json root = ReadJson(outputPath);
    EXPECT_EQ(root.at("schemaVersion").get<int>(), 1);
    EXPECT_EQ(root.at("packageId").get<std::string>(),
              "saga.runtime_smoke.client");
    EXPECT_EQ(root.at("packageKind").get<std::string>(), "client");
    EXPECT_EQ(root.at("buildProfile").get<std::string>(), "dev-client");
    EXPECT_EQ(root.at("targetPlatform").get<std::string>(), "linux");
    EXPECT_EQ(root.at("runtimeCompatibilityVersion").get<std::string>(),
              "0.0.9");
    EXPECT_EQ(root.at("assetIdentityManifest").get<std::string>(),
              "Manifests/asset_identity.json");
    ASSERT_EQ(root.at("assetManifests").size(), 1u);
    EXPECT_EQ(root.at("assetManifests")[0].at("id").get<std::string>(),
              "assets.runtime_smoke");
    EXPECT_EQ(root.at("assetManifests")[0].at("path").get<std::string>(),
              "Manifests/assets.json");
    EXPECT_EQ(root.at("assetManifests")[0].at("hash").get<std::string>(),
              "assets-hash");
    ASSERT_EQ(root.at("artifactManifests").size(), 1u);
    EXPECT_EQ(root.at("artifactManifests")[0].at("id").get<std::string>(),
              "artifacts.runtime_smoke");
    EXPECT_EQ(root.at("packageHash").get<std::string>(), "package-hash");

    const Packages::PackageManifestLoadResult loadResult =
        Packages::PackageManifestLoader::LoadFromFile(outputPath);
    ASSERT_TRUE(loadResult.Succeeded());
    EXPECT_EQ(loadResult.manifest.packageId, "saga.runtime_smoke.client");
    EXPECT_EQ(loadResult.manifest.packageKind, Packages::PackageKind::Client);
    ASSERT_TRUE(loadResult.manifest.assetIdentityManifest.has_value());
    EXPECT_EQ(*loadResult.manifest.assetIdentityManifest,
              "Manifests/asset_identity.json");
    ASSERT_EQ(loadResult.manifest.assetManifests.size(), 1u);
    ASSERT_EQ(loadResult.manifest.artifactManifests.size(), 1u);
}

TEST(PackageManifestWriterTests, WriterOutputPassesStartupGateWithGeneratedManifests)
{
    const std::filesystem::path root =
        TempRoot("saga_package_manifest_writer_startup");
    WriteGeneratedRuntimeSmokePayload(root);

    const std::filesystem::path packageManifestPath =
        root / "package_manifest.client.json";
    const Pipeline::PackageManifestWriteResult writeResult =
        Pipeline::PackageManifestWriter::WriteToFile(
            packageManifestPath,
            RuntimeSmokePackageInput());
    ASSERT_TRUE(writeResult.Succeeded());

    const Startup::RuntimeStartupGateResult startupResult =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(
            StartupOptionsFor(packageManifestPath));

    ASSERT_TRUE(startupResult.Succeeded());
    ASSERT_EQ(startupResult.packageManifest.assetManifests.size(), 1u);

    Resources::RuntimeAssetRegistryBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.validateAssetFiles = true;

    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult bootstrapResult =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                startupResult.packageManifest,
                options);

    ASSERT_TRUE(bootstrapResult.Succeeded());
    EXPECT_EQ(bootstrapResult.registeredAssetCount, 1u);
    EXPECT_EQ(registry.EntryCount(), 1u);
}

TEST(PackageManifestWriterTests, InvalidRequiredFieldsFailBeforeArtifactCreation)
{
    struct Case
    {
        const char* name;
        Pipeline::PackageManifestWriteInput input;
        const char* diagnosticId;
    };

    Pipeline::PackageManifestWriteInput emptyPackageId =
        RuntimeSmokePackageInput();
    emptyPackageId.packageId.clear();

    Pipeline::PackageManifestWriteInput invalidPackageKind =
        RuntimeSmokePackageInput();
    invalidPackageKind.packageKind = Pipeline::PackageManifestKind::Invalid;

    Pipeline::PackageManifestWriteInput emptyBuildProfile =
        RuntimeSmokePackageInput();
    emptyBuildProfile.buildProfile.clear();

    Pipeline::PackageManifestWriteInput emptyTargetPlatform =
        RuntimeSmokePackageInput();
    emptyTargetPlatform.targetPlatform.clear();

    Pipeline::PackageManifestWriteInput emptyRuntimeCompatibility =
        RuntimeSmokePackageInput();
    emptyRuntimeCompatibility.runtimeCompatibilityVersion.clear();

    const std::vector<Case> cases = {
        Case{
            .name = "empty_package_id",
            .input = emptyPackageId,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidPackageId},
        Case{
            .name = "invalid_package_kind",
            .input = invalidPackageKind,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidPackageKind},
        Case{
            .name = "empty_build_profile",
            .input = emptyBuildProfile,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidBuildProfile},
        Case{
            .name = "empty_target_platform",
            .input = emptyTargetPlatform,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidTargetPlatform},
        Case{
            .name = "empty_runtime_compatibility",
            .input = emptyRuntimeCompatibility,
            .diagnosticId = Pipeline::PackageManifestWriterDiagnostics::
                InvalidRuntimeCompatibilityVersion},
    };

    for (const Case& testCase : cases)
    {
        const std::string tempName =
            std::string("saga_package_manifest_writer_") + testCase.name;
        const std::filesystem::path outputPath =
            TempRoot(tempName.c_str()) / "package_manifest.client.json";

        const Pipeline::PackageManifestWriteResult writeResult =
            Pipeline::PackageManifestWriter::WriteToFile(
                outputPath,
                testCase.input);

        EXPECT_FALSE(writeResult.Succeeded()) << testCase.name;
        EXPECT_TRUE(HasDiagnostic(writeResult, testCase.diagnosticId))
            << testCase.name;
        EXPECT_FALSE(std::filesystem::exists(outputPath)) << testCase.name;
    }
}

TEST(PackageManifestWriterTests, UnsafeManifestPathsFailBeforeArtifactCreation)
{
    struct Case
    {
        const char* name;
        Pipeline::PackageManifestWriteInput input;
        const char* diagnosticId;
    };

    Pipeline::PackageManifestWriteInput escapingIdentity =
        RuntimeSmokePackageInput();
    escapingIdentity.assetIdentityManifest = "../asset_identity.json";

    Pipeline::PackageManifestWriteInput emptyAssetPath =
        RuntimeSmokePackageInput();
    emptyAssetPath.assetManifests[0].path.clear();

    Pipeline::PackageManifestWriteInput escapingAssetPath =
        RuntimeSmokePackageInput();
    escapingAssetPath.assetManifests[0].path = "../assets.json";

    Pipeline::PackageManifestWriteInput absoluteArtifactPath =
        RuntimeSmokePackageInput();
    absoluteArtifactPath.artifactManifests.push_back(
        Pipeline::PackageManifestRefWriteInput{
            .id = "artifacts.runtime_smoke",
            .path = "/tmp/artifacts.json"});

    const std::vector<Case> cases = {
        Case{
            .name = "escaping_identity",
            .input = escapingIdentity,
            .diagnosticId = Pipeline::PackageManifestWriterDiagnostics::
                InvalidAssetIdentityManifestPath},
        Case{
            .name = "empty_asset_path",
            .input = emptyAssetPath,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidManifestRefPath},
        Case{
            .name = "escaping_asset_path",
            .input = escapingAssetPath,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidManifestRefPath},
        Case{
            .name = "absolute_artifact_path",
            .input = absoluteArtifactPath,
            .diagnosticId =
                Pipeline::PackageManifestWriterDiagnostics::InvalidManifestRefPath},
    };

    for (const Case& testCase : cases)
    {
        const std::string tempName =
            std::string("saga_package_manifest_writer_") + testCase.name;
        const std::filesystem::path outputPath =
            TempRoot(tempName.c_str()) / "package_manifest.client.json";

        const Pipeline::PackageManifestWriteResult writeResult =
            Pipeline::PackageManifestWriter::WriteToFile(
                outputPath,
                testCase.input);

        EXPECT_FALSE(writeResult.Succeeded()) << testCase.name;
        EXPECT_TRUE(HasDiagnostic(writeResult, testCase.diagnosticId))
            << testCase.name;
        EXPECT_FALSE(std::filesystem::exists(outputPath)) << testCase.name;
    }
}

TEST(PackageManifestWriterTests, DuplicateManifestRefsFailBeforeArtifactCreation)
{
    Pipeline::PackageManifestWriteInput duplicateAssets =
        RuntimeSmokePackageInput();
    duplicateAssets.assetManifests.push_back(
        Pipeline::PackageManifestRefWriteInput{
            .id = "assets.runtime_smoke",
            .path = "Manifests/assets_copy.json"});

    Pipeline::PackageManifestWriteInput duplicateArtifacts =
        RuntimeSmokePackageInput();
    duplicateArtifacts.artifactManifests.push_back(
        Pipeline::PackageManifestRefWriteInput{
            .id = "artifacts.runtime_smoke",
            .path = "Manifests/artifacts.json"});
    duplicateArtifacts.artifactManifests.push_back(
        Pipeline::PackageManifestRefWriteInput{
            .id = "artifacts.runtime_smoke",
            .path = "Manifests/artifacts_copy.json"});

    for (const Pipeline::PackageManifestWriteInput& input :
         {duplicateAssets, duplicateArtifacts})
    {
        const std::filesystem::path outputPath =
            TempRoot("saga_package_manifest_writer_duplicate_refs") /
            "package_manifest.client.json";

        const Pipeline::PackageManifestWriteResult writeResult =
            Pipeline::PackageManifestWriter::WriteToFile(outputPath, input);

        EXPECT_FALSE(writeResult.Succeeded());
        EXPECT_TRUE(HasDiagnostic(
            writeResult,
            Pipeline::PackageManifestWriterDiagnostics::DuplicateManifestRefId));
        EXPECT_FALSE(std::filesystem::exists(outputPath));
    }
}

TEST(PackageManifestWriterTests, WriterDoesNotGenerateReferencedManifests)
{
    const std::filesystem::path root =
        TempRoot("saga_package_manifest_writer_refs_only");
    const std::filesystem::path packageManifestPath =
        root / "package_manifest.client.json";

    const Pipeline::PackageManifestWriteResult writeResult =
        Pipeline::PackageManifestWriter::WriteToFile(
            packageManifestPath,
            RuntimeSmokePackageInput());

    ASSERT_TRUE(writeResult.Succeeded());
    EXPECT_TRUE(std::filesystem::exists(packageManifestPath));
    EXPECT_FALSE(std::filesystem::exists(
        root / "Manifests" / "asset_identity.json"));
    EXPECT_FALSE(std::filesystem::exists(root / "Manifests" / "assets.json"));
}
