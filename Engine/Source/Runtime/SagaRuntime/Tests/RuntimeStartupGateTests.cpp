/// @file RuntimeStartupGateTests.cpp
/// @brief Unit tests for thin runtime startup package validation orchestration.

#include <SagaEngine/Assets/AssetManifestLoader.hpp>
#include <SagaEngine/Artifacts/ArtifactManifestLoader.hpp>
#include <SagaEngine/Packages/PackageManifestLoader.hpp>
#include <SagaEngine/Packages/PackageStartupValidator.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetManifestRegistryAdapter.h>
#include <SagaEngine/Startup/RuntimeStartupGate.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using SagaEngine::Startup::RuntimeStartupDomain;
using SagaEngine::Startup::RuntimeStartupGate;
using SagaEngine::Startup::RuntimeStartupGateOptions;

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] std::string PackageJson(
    const char* packageKind,
    const char* assetManifestPath = "Manifests/assets.json",
    const char* artifactManifestPath = "Manifests/artifacts.json",
    const char* compatibility = "0.0.8")
{
    return std::string(R"({
  "schemaVersion": 1,
  "packageId": "starter.)") + packageKind + R"(",
  "packageKind": ")" + packageKind + R"(",
  "buildProfile": "dev-)" + packageKind + R"(",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": ")" + compatibility + R"(",
  "assetManifests": [
    { "id": "assets.main", "path": ")" + assetManifestPath + R"(" }
  ],
  "artifactManifests": [
    { "id": "artifacts.main", "path": ")" + artifactManifestPath + R"(" }
  ]
})";
}

[[nodiscard]] std::string PackageJsonWithIdentity(
    const char* packageKind,
    const char* identityManifestPath = "Manifests/asset_identity.json",
    const char* assetManifestPath = "Manifests/assets.json",
    const char* artifactManifestPath = "Manifests/artifacts.json")
{
    return std::string(R"({
  "schemaVersion": 1,
  "packageId": "starter.)") + packageKind + R"(",
  "packageKind": ")" + packageKind + R"(",
  "buildProfile": "dev-)" + packageKind + R"(",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetIdentityManifest": ")" + identityManifestPath + R"(",
  "assetManifests": [
    { "id": "assets.main", "path": ")" + assetManifestPath + R"(" }
  ],
  "artifactManifests": [
    { "id": "artifacts.main", "path": ")" + artifactManifestPath + R"(" }
  ]
})";
}

[[nodiscard]] std::string EmptyAssetManifestJson()
{
    return R"({"schemaVersion":1,"assets":[]})";
}

[[nodiscard]] std::string AssetManifestJson(const char* assetPath)
{
    return std::string(R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "texture.hero", "kind": "texture", "path": ")") + assetPath + R"(" }
  ]
})";
}

[[nodiscard]] std::string ArtifactManifestJson(const char* artifactPath)
{
    return std::string(R"({
  "schemaVersion": 1,
  "artifacts": [
    { "id": "quest.graph", "kind": "graph", "path": ")") + artifactPath + R"(" }
  ]
})";
}

[[nodiscard]] std::string AssetIdentityManifestJson(
    const char* assetKey,
    unsigned long long assetId)
{
    return std::string(R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": ")") + assetKey + R"(", "assetId": )" +
           std::to_string(assetId) + R"( }
  ]
})";
}

void WriteValidReferencedManifests(const std::filesystem::path& root)
{
    WriteFile(root / "Manifests" / "assets.json", EmptyAssetManifestJson());
    WriteFile(root / "Manifests" / "artifacts.json",
              ArtifactManifestJson("Artifacts/quest.graph.json"));
    WriteFile(root / "Manifests" / "Artifacts" / "quest.graph.json", "{}");
}

[[nodiscard]] RuntimeStartupGateOptions OptionsFor(
    const std::filesystem::path& packageManifest,
    RuntimeStartupDomain domain)
{
    RuntimeStartupGateOptions options;
    options.packageManifestPath = packageManifest;
    options.expectedDomain = domain;
    return options;
}

} // namespace

TEST(RuntimeStartupGateTests, ValidClientPackageSucceedsForClientDomain)
{
    const auto root = TempRoot("saga_runtime_startup_gate_valid_client");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(root);

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.packageManifest.packageId, "starter.client");
}

TEST(RuntimeStartupGateTests, ValidServerPackageSucceedsForServerDomain)
{
    const auto root = TempRoot("saga_runtime_startup_gate_valid_server");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("server"));
    WriteValidReferencedManifests(root);

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Server));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.packageManifest.packageId, "starter.server");
}

TEST(RuntimeStartupGateTests, DomainMismatchFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_domain_mismatch");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("server"));
    WriteValidReferencedManifests(root);

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Packages::PackageStartupDiagnostics::WrongPackageKind);
}

TEST(RuntimeStartupGateTests, RuntimeCompatibilityMismatchFailsWhenExpectedTokenIsSet)
{
    const auto root = TempRoot("saga_runtime_startup_gate_runtime_mismatch");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client", "Manifests/assets.json",
                                       "Manifests/artifacts.json", "0.0.8"));
    WriteValidReferencedManifests(root);

    auto options = OptionsFor(packagePath, RuntimeStartupDomain::Client);
    options.expectedRuntimeCompatibilityVersion = "0.0.9";

    const auto result = RuntimeStartupGate::ValidatePackageForStartup(options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Packages::PackageStartupDiagnostics::IncompatibleRuntime);
}

TEST(RuntimeStartupGateTests, MissingReferencedManifestFailsWhenValidationIsEnabled)
{
    const auto root = TempRoot("saga_runtime_startup_gate_missing_ref");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 2u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Packages::PackageManifestDiagnostics::FileMissing);
    ASSERT_TRUE(result.diagnostics[0].resolvedPath.has_value());
}

TEST(RuntimeStartupGateTests, ReferencedManifestValidationCanBeSkipped)
{
    const auto root = TempRoot("saga_runtime_startup_gate_skip_refs");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));

    auto options = OptionsFor(packagePath, RuntimeStartupDomain::Client);
    options.validateReferencedManifestFiles = false;

    const auto result = RuntimeStartupGate::ValidatePackageForStartup(options);

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, InvalidAssetManifestFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_invalid_asset_manifest");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteFile(root / "Manifests" / "assets.json",
              R"({"schemaVersion":1,"assets":[{"id":"bad","kind":"source-png","path":"Cooked/bad.bin"}]})");
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Assets::AssetManifestDiagnostics::UnknownKind);
    EXPECT_EQ(result.diagnostics[0].manifestPath, root / "Manifests" / "assets.json");
}

TEST(RuntimeStartupGateTests, MissingCookedAssetFailsWhenAssetFileValidationIsEnabled)
{
    const auto root = TempRoot("saga_runtime_startup_gate_missing_asset_file");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/missing.ktx2"));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Assets::AssetManifestDiagnostics::FileMissing);
    EXPECT_EQ(result.diagnostics[0].manifestPath, root / "Manifests" / "assets.json");
    ASSERT_TRUE(result.diagnostics[0].resolvedPath.has_value());
    EXPECT_EQ(*result.diagnostics[0].resolvedPath,
              root / "Manifests" / "Cooked" / "missing.ktx2");
}

TEST(RuntimeStartupGateTests, MissingCookedAssetIsAllowedWhenAssetFileValidationIsDisabled)
{
    const auto root = TempRoot("saga_runtime_startup_gate_skip_asset_files");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/missing.ktx2"));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    auto options = OptionsFor(packagePath, RuntimeStartupDomain::Client);
    options.validateAssetFiles = false;

    const auto result = RuntimeStartupGate::ValidatePackageForStartup(options);

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, IdentityBackedAssetValidationSucceedsWhenMappingsCoverAssets)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_valid");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/hero.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("texture.hero", 1001));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, MissingIdentityManifestFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_missing");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/hero.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Resources::AssetIdentityManifestDiagnostics::ManifestMissing);
}

TEST(RuntimeStartupGateTests, InvalidIdentityManifestFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_invalid");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/hero.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json", "{");
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Resources::AssetIdentityManifestDiagnostics::ParseFailed);
}

TEST(RuntimeStartupGateTests, MissingIdentityMappingFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_missing_mapping");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/hero.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("texture.other", 1001));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Resources::AssetManifestRegistryAdapterDiagnostics::
                  MissingAssetIdMapping);
    ASSERT_TRUE(result.diagnostics[0].resourceId.has_value());
    EXPECT_EQ(*result.diagnostics[0].resourceId, "texture.hero");
    ASSERT_TRUE(result.diagnostics[0].itemIndex.has_value());
    EXPECT_EQ(*result.diagnostics[0].itemIndex, 0u);
}

TEST(RuntimeStartupGateTests, IdentityValidationCanBeSkippedWithReferencedManifestValidation)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_skip_refs");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));

    auto options = OptionsFor(packagePath, RuntimeStartupDomain::Client);
    options.validateReferencedManifestFiles = false;

    const auto result = RuntimeStartupGate::ValidatePackageForStartup(options);

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, IdentityValidationCanSkipCookedAssetFileChecks)
{
    const auto root = TempRoot("saga_runtime_startup_gate_identity_skip_asset_files");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJsonWithIdentity("client"));
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("Cooked/missing.ktx2"));
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("texture.hero", 1001));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    auto options = OptionsFor(packagePath, RuntimeStartupDomain::Client);
    options.validateAssetFiles = false;

    const auto result = RuntimeStartupGate::ValidatePackageForStartup(options);

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, MissingArtifactFileFailsStartup)
{
    const auto root = TempRoot("saga_runtime_startup_gate_missing_artifact");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteFile(root / "Manifests" / "assets.json", EmptyAssetManifestJson());
    WriteFile(root / "Manifests" / "artifacts.json",
              ArtifactManifestJson("Artifacts/missing.graph.json"));

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaEngine::Artifacts::ArtifactManifestDiagnostics::FileMissing);
    ASSERT_TRUE(result.diagnostics[0].resourceId.has_value());
    EXPECT_EQ(*result.diagnostics[0].resourceId, "quest.graph");
    ASSERT_TRUE(result.diagnostics[0].resolvedPath.has_value());
}

TEST(RuntimeStartupGateTests, RelativeReferencedManifestPathsResolveFromPackageManifestParent)
{
    const auto root = TempRoot("saga_runtime_startup_gate_relative_refs");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client", "Nested/assets.json",
                                       "Nested/artifacts.json"));
    WriteFile(root / "Nested" / "assets.json", EmptyAssetManifestJson());
    WriteFile(root / "Nested" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_TRUE(result.Succeeded());
}

TEST(RuntimeStartupGateTests, MultipleDiagnosticsAreAccumulated)
{
    const auto root = TempRoot("saga_runtime_startup_gate_multiple_diagnostics");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, R"({
  "schemaVersion": 1,
  "packageId": "starter.client",
  "packageKind": "client",
  "buildProfile": "dev-client",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.a", "path": "Missing/assets-a.json" },
    { "id": "assets.b", "path": "Missing/assets-b.json" }
  ],
  "artifactManifests": [
    { "id": "artifacts.a", "path": "Missing/artifacts-a.json" }
  ]
})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.size(), 3u);
    for (const auto& diagnostic : result.diagnostics)
    {
        EXPECT_EQ(diagnostic.manifestPath, packagePath);
        EXPECT_TRUE(diagnostic.resolvedPath.has_value());
    }
}

TEST(RuntimeStartupGateTests, DiagnosticsIncludeManifestAndResolvedPathsWhereApplicable)
{
    const auto root = TempRoot("saga_runtime_startup_gate_diagnostic_paths");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client", "Missing/assets.json",
                                       "Manifests/artifacts.json"));
    WriteFile(root / "Manifests" / "artifacts.json", R"({"schemaVersion":1,"artifacts":[]})");

    const auto result =
        RuntimeStartupGate::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].manifestPath, packagePath);
    ASSERT_TRUE(result.diagnostics[0].resolvedPath.has_value());
    EXPECT_EQ(*result.diagnostics[0].resolvedPath, root / "Missing" / "assets.json");
}
