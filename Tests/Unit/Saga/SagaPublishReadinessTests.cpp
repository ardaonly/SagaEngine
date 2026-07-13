/// @file SagaPublishReadinessTests.cpp
/// @brief Focused publish readiness package/asset identity report tests.

#include "Projects/SagaProjectSystem.h"
#include "SagaPublishReadiness.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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
    std::ofstream out(path, std::ios::trunc);
    out << text;
}

void WriteAssetIdentityManifest(const fs::path& projectRoot)
{
    WriteFile(projectRoot / "Build" / "Manifests" / "asset_identity.json",
              R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.publish.hero", "assetId": 7001 }
  ]
})");
}

void WriteValidAssetManifest(const fs::path& projectRoot)
{
    WriteFile(projectRoot / "Build" / "Manifests" / "Cooked" /
                  "hero_texture.ktx2",
              "publish-ready-texture");
    WriteFile(projectRoot / "Build" / "Manifests" / "assets.json",
              R"({
  "schemaVersion": 1,
  "assets": [
    {
      "id": "texture.publish.hero",
      "kind": "texture",
      "path": "Cooked/hero_texture.ktx2",
      "hash": "sha256-publish-ready-texture"
    }
  ]
})");
}

void WritePackageManifest(const fs::path& projectRoot,
                          const std::string& slot,
                          const std::string& assetManifestPath =
                              "Build/Manifests/assets.json",
                          const std::string& identityManifestPath =
                              "Build/Manifests/asset_identity.json")
{
    const fs::path packagePath = projectRoot / "Build" / "Manifests" /
        ("package_manifest." + slot + ".json");
    WriteFile(packagePath, R"({
  "schemaVersion": 1,
  "packageId": "publish.)" + slot + R"(",
  "packageKind": ")" + slot + R"(",
  "buildProfile": "shipping-full",
  "targetPlatform": "linux-x64",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetIdentityManifest": ")" + identityManifestPath + R"(",
  "assetManifests": [
    { "id": "assets.main", "path": ")" + assetManifestPath + R"(" }
  ],
  "artifactManifests": [],
  "packageHash": "sha256-publish-ready-package"
})");
}

void WriteValidPackageInputs(const fs::path& projectRoot)
{
    WriteAssetIdentityManifest(projectRoot);
    WriteValidAssetManifest(projectRoot);
    WritePackageManifest(projectRoot, "client");
    WritePackageManifest(projectRoot, "server");
}

[[nodiscard]] SagaProjectResult CreateProject(const fs::path& root,
                                              const std::string& name)
{
    SagaProjectSystem projects(root / "recent_projects.json");
    return projects.CreateProject(root, name);
}

[[nodiscard]] nlohmann::json ReadJson(const fs::path& path)
{
    std::ifstream input(path);
    return nlohmann::json::parse(input);
}

[[nodiscard]] const nlohmann::json& ClientCoverage(
    const nlohmann::json& report)
{
    const nlohmann::json& coverage =
        report["packageAssetIdentityCoverage"];
    EXPECT_EQ(coverage[0]["packageSlot"], "client");
    return coverage[0];
}

[[nodiscard]] const SagaPublishPackageAssetIdentityCoverage& ClientCoverage(
    const SagaPublishReadinessResult& result)
{
    EXPECT_EQ(result.packageAssetIdentityCoverage[0].packageSlot, "client");
    return result.packageAssetIdentityCoverage[0];
}

} // namespace

TEST(SagaPublishReadinessTests,
     ValidGeneratedPackageManifestsReportPackageAssetAndIdentityCoverage)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_valid_coverage_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishReadinessCoverageProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Ready);
    ASSERT_EQ(result.packageAssetIdentityCoverage.size(), 2u);
    const SagaPublishPackageAssetIdentityCoverage& client =
        ClientCoverage(result);
    EXPECT_TRUE(client.packageManifestExists);
    EXPECT_TRUE(client.packageManifestLoads);
    EXPECT_EQ(client.packageId, "publish.client");
    EXPECT_EQ(client.packageKind, "client");
    EXPECT_TRUE(client.assetIdentityManifest.referenced);
    EXPECT_TRUE(client.assetIdentityManifest.exists);
    EXPECT_TRUE(client.assetIdentityManifest.loads);
    EXPECT_EQ(client.assetIdentityManifest.mappingCount, 1u);
    ASSERT_EQ(client.assetManifests.size(), 1u);
    EXPECT_EQ(client.assetManifests[0].id, "assets.main");
    EXPECT_TRUE(client.assetManifests[0].exists);
    EXPECT_TRUE(client.assetManifests[0].loads);
    EXPECT_EQ(client.assetManifests[0].assetCount, 1u);

    const nlohmann::json report = ReadJson(result.reportPath);
    EXPECT_EQ(report["status"], "ready");
    ASSERT_EQ(report["packageAssetIdentityCoverage"].size(), 2u);
    EXPECT_EQ(report["packageAssetIdentityCoverage"][0]["packageSlot"],
              "client");
    EXPECT_EQ(report["packageAssetIdentityCoverage"][1]["packageSlot"],
              "server");
    const nlohmann::json& clientJson = ClientCoverage(report);
    EXPECT_EQ(clientJson["packageId"], "publish.client");
    EXPECT_EQ(clientJson["assetIdentityManifest"]["mappingCount"], 1u);
    EXPECT_EQ(clientJson["assetManifests"][0]["assetCount"], 1u);
}

TEST(SagaPublishReadinessTests,
     MissingPackageManifestRemainsBlockerAndAppearsInCoverage)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_missing_package_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishMissingPackageProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    fs::remove(created.manifest.root / "Build" / "Manifests" /
               "package_manifest.client.json");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Blocked);
    const SagaPublishPackageAssetIdentityCoverage& client =
        ClientCoverage(result);
    EXPECT_FALSE(client.packageManifestExists);
    EXPECT_FALSE(client.packageManifestLoads);
    ASSERT_EQ(client.diagnostics.size(), 1u);
    EXPECT_EQ(client.diagnostics[0].code, "Runtime.Package.ManifestMissing");
}

TEST(SagaPublishReadinessTests,
     MissingAssetManifestReferenceIsReportedDeterministically)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_missing_asset_manifest_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishMissingAssetManifestProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    fs::remove(created.manifest.root / "Build" / "Manifests" / "assets.json");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Blocked);
    const SagaPublishPackageAssetIdentityCoverage& client =
        ClientCoverage(result);
    ASSERT_EQ(client.assetManifests.size(), 1u);
    EXPECT_FALSE(client.assetManifests[0].exists);
    EXPECT_FALSE(client.assetManifests[0].loads);
    ASSERT_EQ(client.assetManifests[0].diagnostics.size(), 1u);
    EXPECT_EQ(client.assetManifests[0].diagnostics[0].code,
              "Runtime.Asset.ManifestMissing");
    ASSERT_TRUE(client.assetManifests[0].diagnostics[0].referenceIndex);
    EXPECT_EQ(*client.assetManifests[0].diagnostics[0].referenceIndex, 0u);
}

TEST(SagaPublishReadinessTests,
     MissingReferencedIdentityManifestIsReportedDeterministically)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_missing_identity_manifest_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishMissingIdentityManifestProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    fs::remove(created.manifest.root / "Build" / "Manifests" /
               "asset_identity.json");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Blocked);
    const SagaPublishPackageAssetIdentityCoverage& client =
        ClientCoverage(result);
    EXPECT_TRUE(client.assetIdentityManifest.referenced);
    EXPECT_FALSE(client.assetIdentityManifest.exists);
    EXPECT_FALSE(client.assetIdentityManifest.loads);
    ASSERT_EQ(client.assetIdentityManifest.diagnostics.size(), 1u);
    EXPECT_EQ(client.assetIdentityManifest.diagnostics[0].code,
              "Runtime.AssetIdentityManifest.ManifestMissing");
}

TEST(SagaPublishReadinessTests,
     MissingCookedAssetIsCoverageOnlyAndDoesNotAddBlocker)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_missing_cooked_asset_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishMissingCookedAssetProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    fs::remove(created.manifest.root / "Build" / "Manifests" / "Cooked" /
               "hero_texture.ktx2");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Ready);
    const SagaPublishPackageAssetIdentityCoverage& client =
        ClientCoverage(result);
    ASSERT_EQ(client.assetManifests.size(), 1u);
    EXPECT_TRUE(client.assetManifests[0].exists);
    EXPECT_FALSE(client.assetManifests[0].loads);
    ASSERT_EQ(client.assetManifests[0].diagnostics.size(), 1u);
    EXPECT_EQ(client.assetManifests[0].diagnostics[0].code,
              "Runtime.Asset.FileMissing");
}

TEST(SagaPublishReadinessTests,
     InvalidAssetManifestDiagnosticIsSerializedWithoutHardGateExpansion)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_invalid_asset_manifest_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishInvalidAssetManifestProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    WriteFile(created.manifest.root / "Build" / "Manifests" / "assets.json",
              "[]");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    const nlohmann::json report = ReadJson(result.reportPath);
    const nlohmann::json& clientJson = ClientCoverage(report);
    EXPECT_EQ(clientJson["assetManifests"][0]["loads"], false);
    ASSERT_EQ(clientJson["assetManifests"][0]["diagnostics"].size(), 1u);
    EXPECT_EQ(clientJson["assetManifests"][0]["diagnostics"][0]["code"],
              "Runtime.Asset.ParseFailed");
    EXPECT_EQ(report["status"], "ready");
}

TEST(SagaPublishReadinessTests,
     UnknownAssetKindIsCoverageOnlyAndDoesNotAddBlocker)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_unknown_asset_kind_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishUnknownAssetKindProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    WriteFile(created.manifest.root / "Build" / "Manifests" / "assets.json",
              R"({
  "schemaVersion": 1,
  "assets": [
    {
      "id": "texture.publish.hero",
      "kind": "source-png",
      "path": "Cooked/hero_texture.ktx2"
    }
  ]
})");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    const nlohmann::json report = ReadJson(result.reportPath);
    const nlohmann::json& clientJson = ClientCoverage(report);
    EXPECT_EQ(clientJson["assetManifests"][0]["loads"], false);
    ASSERT_EQ(clientJson["assetManifests"][0]["diagnostics"].size(), 1u);
    EXPECT_EQ(clientJson["assetManifests"][0]["diagnostics"][0]["code"],
              "Runtime.Asset.UnknownKind");
    EXPECT_EQ(clientJson["assetManifests"][0]["diagnostics"][0]["field"],
              "kind");
    EXPECT_EQ(report["status"], "ready");
}

TEST(SagaPublishReadinessTests,
     DuplicateIdentityMappingIsCoverageOnlyAndDoesNotAddBlocker)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_duplicate_identity_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishDuplicateIdentityProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);
    WriteFile(created.manifest.root / "Build" / "Manifests" /
                  "asset_identity.json",
              R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.publish.hero", "assetId": 7001 },
    { "assetKey": "texture.publish.hero", "assetId": 7002 }
  ]
})");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    const nlohmann::json report = ReadJson(result.reportPath);
    const nlohmann::json& clientJson = ClientCoverage(report);
    EXPECT_EQ(clientJson["assetIdentityManifest"]["loads"], false);
    ASSERT_EQ(clientJson["assetIdentityManifest"]["diagnostics"].size(), 1u);
    EXPECT_EQ(clientJson["assetIdentityManifest"]["diagnostics"][0]["code"],
              "Runtime.AssetIdentityManifest.DuplicateAssetKey");
    EXPECT_EQ(clientJson["assetIdentityManifest"]["diagnostics"][0]["field"],
              "assetKey");
    EXPECT_EQ(report["status"], "ready");
}

TEST(SagaPublishReadinessTests, ReportJsonIsDeterministicAndMachineReadable)
{
    const fs::path root =
        MakeTempDir("saga_publish_readiness_deterministic_report_test");
    const SagaProjectResult created =
        CreateProject(root, "PublishDeterministicReportProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPackageInputs(created.manifest.root);

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult first = service.Check(request);
    const nlohmann::json firstReport = ReadJson(first.reportPath);
    const SagaPublishReadinessResult second = service.Check(request);
    const nlohmann::json secondReport = ReadJson(second.reportPath);

    EXPECT_EQ(firstReport.dump(), secondReport.dump());
    EXPECT_EQ(firstReport["schemaVersion"], 1);
    EXPECT_TRUE(firstReport.contains("reportId"));
    EXPECT_TRUE(firstReport.contains("buildId"));
    EXPECT_TRUE(firstReport.contains("packageId"));
    EXPECT_TRUE(firstReport.contains("status"));
    EXPECT_TRUE(firstReport.contains("diagnosticSummary"));
    EXPECT_TRUE(firstReport.contains("metadata"));
    EXPECT_TRUE(firstReport.contains("blockers"));
    EXPECT_TRUE(firstReport.contains("readiness"));
    EXPECT_TRUE(firstReport.contains("packageAssetIdentityCoverage"));
    EXPECT_EQ(firstReport["readiness"]["status"], firstReport["status"]);
    EXPECT_TRUE(firstReport["blockers"].is_array());
    EXPECT_TRUE(firstReport["readiness"]["blockers"].is_array());
    ASSERT_TRUE(firstReport["packageAssetIdentityCoverage"].is_array());
    ASSERT_EQ(firstReport["packageAssetIdentityCoverage"].size(), 2u);
    EXPECT_EQ(firstReport["packageAssetIdentityCoverage"][0]["packageSlot"],
              "client");
    EXPECT_EQ(firstReport["packageAssetIdentityCoverage"][1]["packageSlot"],
              "server");
    const nlohmann::json& clientCoverage =
        firstReport["packageAssetIdentityCoverage"][0];
    EXPECT_TRUE(clientCoverage.contains("packageManifestPath"));
    EXPECT_TRUE(clientCoverage.contains("packageManifestExists"));
    EXPECT_TRUE(clientCoverage.contains("packageManifestLoads"));
    EXPECT_TRUE(clientCoverage.contains("packageId"));
    EXPECT_TRUE(clientCoverage.contains("packageKind"));
    EXPECT_TRUE(clientCoverage.contains("assetIdentityManifest"));
    EXPECT_TRUE(clientCoverage["assetManifests"].is_array());
    EXPECT_TRUE(clientCoverage["diagnostics"].is_array());
    EXPECT_TRUE(clientCoverage["assetIdentityManifest"].contains("referenced"));
    EXPECT_TRUE(clientCoverage["assetIdentityManifest"].contains("exists"));
    EXPECT_TRUE(clientCoverage["assetIdentityManifest"].contains("loads"));
    EXPECT_TRUE(clientCoverage["assetIdentityManifest"].contains("mappingCount"));
}
