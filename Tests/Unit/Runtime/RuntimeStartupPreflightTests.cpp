/// @file RuntimeStartupPreflightTests.cpp
/// @brief Focused tests for the Runtime-owned startup preflight facade.

#include <SagaRuntime/RuntimeStartupPreflight.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using SagaRuntime::RuntimeStartupDomain;
using SagaRuntime::RuntimeStartupPreflight;
using SagaRuntime::RuntimeStartupPreflightDiagnostics::PackageManifestMissing;
using SagaRuntime::RuntimeStartupPreflightOptions;

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

[[nodiscard]] std::string PackageJson(const char* packageKind)
{
    return std::string(R"({
  "schemaVersion": 1,
  "packageId": "starter.)") + packageKind + R"(",
  "packageKind": ")" + packageKind + R"(",
  "buildProfile": "dev-)" + packageKind + R"(",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.main", "path": "Manifests/assets.json" }
  ],
  "artifactManifests": [
    { "id": "artifacts.main", "path": "Manifests/artifacts.json" }
  ]
})";
}

void WriteValidReferencedManifests(const std::filesystem::path& root)
{
    WriteFile(root / "Manifests" / "assets.json",
              R"({"schemaVersion":1,"assets":[]})");
    WriteFile(root / "Manifests" / "artifacts.json", R"({
  "schemaVersion": 1,
  "artifacts": [
    { "id": "quest.graph", "kind": "graph", "path": "Artifacts/quest.graph.json" }
  ]
})");
    WriteFile(root / "Manifests" / "Artifacts" / "quest.graph.json", "{}");
}

[[nodiscard]] RuntimeStartupPreflightOptions OptionsFor(
    const std::filesystem::path& packagePath,
    RuntimeStartupDomain domain = RuntimeStartupDomain::Client)
{
    RuntimeStartupPreflightOptions options;
    options.packageManifestPath = packagePath;
    options.expectedDomain = domain;
    return options;
}

} // namespace

TEST(RuntimeStartupPreflightTests, MissingPackageManifestCanBeSkippedForDev)
{
    RuntimeStartupPreflightOptions options;
    options.allowMissingPackageManifestForDev = true;

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(options);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.validationSkipped);
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(RuntimeStartupPreflightTests, MissingPackageManifestCanFailWhenDevSkipDisabled)
{
    RuntimeStartupPreflightOptions options;
    options.allowMissingPackageManifestForDev = false;

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.validationSkipped);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().diagnosticId, PackageManifestMissing);
}

TEST(RuntimeStartupPreflightTests, ValidClientPackageSucceeds)
{
    const auto root = TempRoot("saga_runtime_preflight_valid_client");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(root);

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(
            OptionsFor(packagePath));

    EXPECT_TRUE(result.Succeeded());
    EXPECT_FALSE(result.validationSkipped);
}

TEST(RuntimeStartupPreflightTests, ServerPackageRejectedForClientDomain)
{
    const auto root = TempRoot("saga_runtime_preflight_domain_mismatch");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("server"));
    WriteValidReferencedManifests(root);

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(
            OptionsFor(packagePath, RuntimeStartupDomain::Client));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_FALSE(result.diagnostics.front().diagnosticId.empty());
}

TEST(RuntimeStartupPreflightTests, MissingReferencedManifestProducesDiagnostic)
{
    const auto root = TempRoot("saga_runtime_preflight_missing_ref");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(
            OptionsFor(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(result.diagnostics.front().resolvedPath.has_value());
}

TEST(RuntimeStartupPreflightTests, ReferencedManifestValidationCanBeSkipped)
{
    const auto root = TempRoot("saga_runtime_preflight_skip_refs");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));

    auto options = OptionsFor(packagePath);
    options.validateReferencedManifestFiles = false;

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(options);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(RuntimeStartupPreflightTests, ExplicitPackageBaseDirectoryResolvesReferences)
{
    const auto root = TempRoot("saga_runtime_preflight_explicit_base");
    const auto packageRoot = root / "Package";
    const auto packageBase = root / "Base";
    const auto packagePath = packageRoot / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(packageBase);

    auto options = OptionsFor(packagePath);
    options.packageBaseDirectory = packageBase;

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(options);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_FALSE(result.validationSkipped);
}

TEST(RuntimeStartupPreflightTests, EmptyPackageBaseDirectoryKeepsManifestFolderBehavior)
{
    const auto root = TempRoot("saga_runtime_preflight_default_base");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(root);

    auto options = OptionsFor(packagePath);
    options.packageBaseDirectory.clear();

    const auto result =
        RuntimeStartupPreflight::ValidatePackageForStartup(options);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_FALSE(result.validationSkipped);
}
