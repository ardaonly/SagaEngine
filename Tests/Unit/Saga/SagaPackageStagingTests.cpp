/// @file SagaPackageStagingTests.cpp
/// @brief Focused tests for Saga package staging without the full product shell.

#include "SagaPackageStaging.h"
#include "SagaProjectSystem.h"
#include "SagaPublishReadiness.h"

#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace fs = std::filesystem;
using namespace SagaProduct;

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
