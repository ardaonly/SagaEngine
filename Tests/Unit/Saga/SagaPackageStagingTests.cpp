/// @file SagaPackageStagingTests.cpp
/// @brief Focused tests for Saga package staging without the full product shell.

#include "SagaPackageStaging.h"
#include "Projects/SagaProjectSystem.h"
#include "SagaPublishReadiness.h"

#include <SagaAssetPipeline/Assets/AssetManifestWriter.hpp>
#include <SagaAssetPipeline/Identity/AssetIdentityManifestWriter.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace fs = std::filesystem;
namespace Pipeline = SagaAssetPipeline;
namespace Packages = SagaEngine::Packages;
namespace Resources = SagaEngine::Resources;
namespace Startup = SagaEngine::Startup;
using namespace SagaProduct;

constexpr const char* kGeneratedAssetKey = "texture.package_stage.generated";
constexpr Resources::AssetId kGeneratedAssetId = 5101;

[[nodiscard]] fs::path MakeTempDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

void WriteGeneratedAssetManifests(const fs::path& projectRoot)
{
    WriteFile(projectRoot / "Build" / "Manifests" / "Cooked" /
                  "runtime_smoke_texture.ktx2",
              "staged-runtime-smoke-texture");

    Pipeline::AssetIdentityMapping mapping;
    mapping.assetKey = kGeneratedAssetKey;
    mapping.assetId = kGeneratedAssetId;
    const Pipeline::AssetIdentityManifestWriteResult identityWrite =
        Pipeline::AssetIdentityManifestWriter::WriteToFile(
            projectRoot / "Build" / "Manifests" / "asset_identity.json",
            {mapping});
    ASSERT_TRUE(identityWrite.Succeeded());

    Pipeline::AssetManifestWriterEntry asset;
    asset.id = kGeneratedAssetKey;
    asset.kind = "texture";
    asset.path = "Cooked/runtime_smoke_texture.ktx2";
    asset.hash = "sha256-package-stage-generated";
    asset.memoryEstimateBytes = 16;
    asset.streamingGroup = "package-stage";
    asset.platform = "linux-x64";
    asset.profile = "shipping-full";
    const Pipeline::AssetManifestWriteResult assetWrite =
        Pipeline::AssetManifestWriter::WriteToFile(
            projectRoot / "Build" / "Manifests" / "assets.json",
            {asset});
    ASSERT_TRUE(assetWrite.Succeeded());
}

} // namespace

TEST(SagaPackageStagingTest, StagesPackageManifestsForPublishReadiness)
{
    const fs::path root = MakeTempDir("saga_package_stage_focused_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PackageStageProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteFile(
        created.manifest.root / "Build" / "Manifests" / "assets.json",
        "{}");
    WriteFile(
        created.manifest.root / "Build" / "Manifests" /
            "artifact_manifest.json",
        "{}");

    SagaPackageStagingRequest request;
    request.projectRoot = created.manifest.root;
    request.profile = "shipping-full";
    request.targetPlatform = "linux-x64";
    request.runtimeCompatibilityVersion = "0.0.8";

    SagaPackageStagingService service;
    const SagaPackageStagingResult result = service.Stage(request);

    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_TRUE(fs::exists(result.paths.clientPackageManifest));
    EXPECT_TRUE(fs::exists(result.paths.serverPackageManifest));
    EXPECT_TRUE(fs::exists(result.paths.reportPath));

    SagaEngine::Packages::PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = created.manifest.root;
    const auto client =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            result.paths.clientPackageManifest,
            options);
    const auto server =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            result.paths.serverPackageManifest,
            options);

    ASSERT_TRUE(client.Succeeded());
    ASSERT_TRUE(server.Succeeded());
    EXPECT_EQ(client.manifest.packageId,
              created.manifest.projectId + ".client.shipping-full");
    EXPECT_EQ(server.manifest.packageId,
              created.manifest.projectId + ".server.shipping-full");
    ASSERT_EQ(client.manifest.assetManifests.size(), 1u);
    EXPECT_EQ(client.manifest.assetManifests[0].path,
              "Build/Manifests/assets.json");
    ASSERT_EQ(client.manifest.artifactManifests.size(), 1u);
    EXPECT_EQ(client.manifest.artifactManifests[0].path,
              "Build/Manifests/artifact_manifest.json");

    SagaPublishReadinessRequest publishRequest;
    publishRequest.projectRoot = created.manifest.root;
    SagaPublishReadinessService publishService;
    const SagaPublishReadinessResult publish =
        publishService.Check(publishRequest);
    EXPECT_TRUE(publish.ok);
    EXPECT_EQ(publish.report.readiness.status,
              SagaShared::Publish::PublishStatus::Ready);
}

TEST(SagaPackageStagingTest,
     GeneratedAssetPipelineManifestsStageAndBootstrapRuntimeRegistry)
{
    const fs::path root =
        MakeTempDir("saga_package_stage_generated_manifest_bridge_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PackageStageGeneratedProject");
    ASSERT_TRUE(created.ok) << created.error;

    WriteGeneratedAssetManifests(created.manifest.root);

    SagaPackageStagingRequest request;
    request.projectRoot = created.manifest.root;
    request.profile = "shipping-full";
    request.targetPlatform = "linux-x64";
    request.runtimeCompatibilityVersion = "0.0.8";

    SagaPackageStagingService service;
    const SagaPackageStagingResult result = service.Stage(request);

    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.diagnostics.empty());

    Packages::PackageManifestLoadOptions loadOptions;
    loadOptions.validateReferencedManifestFiles = true;
    loadOptions.packageBaseDirectory = created.manifest.root;
    const Packages::PackageManifestLoadResult loaded =
        Packages::PackageManifestLoader::LoadFromFile(
            result.paths.clientPackageManifest,
            loadOptions);
    ASSERT_TRUE(loaded.Succeeded());
    ASSERT_TRUE(loaded.manifest.assetIdentityManifest.has_value());
    EXPECT_EQ(*loaded.manifest.assetIdentityManifest,
              "Build/Manifests/asset_identity.json");
    ASSERT_EQ(loaded.manifest.assetManifests.size(), 1u);
    EXPECT_EQ(loaded.manifest.assetManifests[0].path,
              "Build/Manifests/assets.json");

    Startup::RuntimeStartupGateOptions startupOptions;
    startupOptions.packageManifestPath = result.paths.clientPackageManifest;
    startupOptions.packageBaseDirectory = created.manifest.root;
    startupOptions.expectedDomain = Startup::RuntimeStartupDomain::Client;
    startupOptions.expectedRuntimeCompatibilityVersion =
        request.runtimeCompatibilityVersion;
    startupOptions.validateReferencedManifestFiles = true;
    startupOptions.validateAssetFiles = true;
    const Startup::RuntimeStartupGateResult startup =
        Startup::RuntimeStartupGate::ValidatePackageForStartup(startupOptions);
    ASSERT_TRUE(startup.Succeeded());

    Resources::RuntimeAssetRegistryBootstrapOptions bootstrapOptions;
    bootstrapOptions.packageManifestPath = result.paths.clientPackageManifest;
    bootstrapOptions.packageBaseDirectory = created.manifest.root;
    bootstrapOptions.validateAssetFiles = true;
    Resources::AssetRegistry registry;
    const Resources::RuntimeAssetRegistryBootstrapResult bootstrap =
        Resources::RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry,
                loaded.manifest,
                bootstrapOptions);

    ASSERT_TRUE(bootstrap.Succeeded());
    EXPECT_EQ(bootstrap.registeredAssetCount, 1u);
    EXPECT_EQ(registry.EntryCount(), 1u);

    const Resources::AssetRegistryEntry* byKey =
        registry.FindByKey(kGeneratedAssetKey);
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, kGeneratedAssetId);
    EXPECT_EQ(byKey->kind, Resources::AssetKind::Texture);

    const Resources::AssetRegistryEntry* byId =
        registry.Find(kGeneratedAssetId);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, kGeneratedAssetKey);
}

TEST(SagaPackageStagingTest, MissingManifestInputsBlockStaging)
{
    const fs::path root =
        MakeTempDir("saga_package_stage_focused_missing_inputs_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PackageStageMissingInputsProject");
    ASSERT_TRUE(created.ok) << created.error;

    SagaPackageStagingRequest request;
    request.projectRoot = created.manifest.root;

    SagaPackageStagingService service;
    const SagaPackageStagingResult result = service.Stage(request);

    EXPECT_FALSE(result.ok);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaProductDiagnostics::PackageStageInputsMissing);
    EXPECT_TRUE(fs::exists(result.paths.reportPath));
    EXPECT_FALSE(fs::exists(result.paths.clientPackageManifest));
    EXPECT_FALSE(fs::exists(result.paths.serverPackageManifest));
}

TEST(SagaPackageStagingTest, ServerOnlyScriptArtifactIsServerPackageOnly)
{
    const fs::path root =
        MakeTempDir("saga_package_stage_focused_script_artifact_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PackageStageScriptProject");
    ASSERT_TRUE(created.ok) << created.error;

    WriteFile(
        created.manifest.root / "Build" / "Artifacts" / "Scripts" /
            "SagaProjectScripts.scripts.dll",
        "compiled-script-placeholder");
    WriteFile(
        created.manifest.root / "Build" / "Manifests" / "script_artifacts.json",
        R"({
  "schemaVersion": 1,
  "targetFramework": "net10.0",
  "artifacts": [
    {
      "artifactId": "artifact://scripts/gameplay/inventory",
      "scriptId": "script://gameplay/inventory",
      "assemblyPath": "Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll",
      "runtimeConfigPath": "Build/Artifacts/Scripts/SagaProjectScripts.scripts.runtimeconfig.json",
      "hash": "sha256-test",
      "authority": "ServerOnly",
      "packageDestinationIntent": "server",
      "requestedCapabilities": ["Gameplay.Inventory.Write"],
      "grantedCapabilities": [],
      "bindingIds": ["script://gameplay/inventory#Game.Inventory.GiveItem"],
      "sourceFiles": ["Scripts/Inventory.cs"]
    }
  ]
})");

    SagaPackageStagingRequest request;
    request.projectRoot = created.manifest.root;

    SagaPackageStagingService service;
    const SagaPackageStagingResult result = service.Stage(request);

    ASSERT_TRUE(result.ok);
    SagaEngine::Packages::PackageManifestLoadOptions options;
    options.validateReferencedManifestFiles = true;
    options.packageBaseDirectory = created.manifest.root;
    const auto client =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            result.paths.clientPackageManifest,
            options);
    const auto server =
        SagaEngine::Packages::PackageManifestLoader::LoadFromFile(
            result.paths.serverPackageManifest,
            options);

    ASSERT_TRUE(client.Succeeded());
    ASSERT_TRUE(server.Succeeded());
    EXPECT_TRUE(client.manifest.artifactManifests.empty());
    ASSERT_EQ(server.manifest.artifactManifests.size(), 1u);
    EXPECT_EQ(server.manifest.artifactManifests[0].id, "scripts.server");
    EXPECT_EQ(server.manifest.artifactManifests[0].path,
              "Build/Manifests/artifact_manifest.scripts.server.json");

    std::ifstream input(created.manifest.root /
        "Build" / "Manifests" / "artifact_manifest.scripts.server.json");
    const nlohmann::json scriptManifest = nlohmann::json::parse(input);
    ASSERT_EQ(scriptManifest["artifacts"].size(), 1u);
    EXPECT_EQ(scriptManifest["artifacts"][0]["kind"], "script");
    EXPECT_EQ(scriptManifest["artifacts"][0]["path"],
              "Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll");
}
