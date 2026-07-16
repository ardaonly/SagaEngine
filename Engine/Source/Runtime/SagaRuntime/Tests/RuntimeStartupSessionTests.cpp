/// @file RuntimeStartupSessionTests.cpp
/// @brief Focused tests for Runtime startup session preparation.

#include <SagaRuntime/RuntimeStartupSession.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using SagaRuntime::RuntimeLaunchOptions;
using SagaRuntime::RuntimeStartupDomain;
using SagaRuntime::RuntimeStartupSession;

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

} // namespace

TEST(RuntimeStartupSessionTests, DefaultLaunchProducesSkippedClientDescriptor)
{
    const auto report = RuntimeStartupSession::Prepare(RuntimeLaunchOptions{});

    EXPECT_TRUE(report.Succeeded());
    EXPECT_TRUE(report.preflight.validationSkipped);
    EXPECT_EQ(report.sessionDescriptor.serverHost, "127.0.0.1");
    EXPECT_EQ(report.sessionDescriptor.serverPort, 7777);
    EXPECT_FALSE(report.sessionDescriptor.headless);
    EXPECT_TRUE(report.sessionDescriptor.enableStartupUi);
    EXPECT_EQ(report.sessionDescriptor.domain, RuntimeStartupDomain::Client);
}

TEST(RuntimeStartupSessionTests, PreservesNormalizedLaunchIntent)
{
    RuntimeLaunchOptions options;
    options.serverHost = "10.1.2.3";
    options.serverPort = 7788;
    options.headless = true;
    options.enableStartupUi = false;
    options.startupContentRoot = "CustomContent";
    options.packageBaseDirectory = "PackageBase";
    options.expectedDomain = RuntimeStartupDomain::Server;

    const auto report = RuntimeStartupSession::Prepare(options);

    EXPECT_TRUE(report.Succeeded());
    EXPECT_EQ(report.sessionDescriptor.serverHost, options.serverHost);
    EXPECT_EQ(report.sessionDescriptor.serverPort, options.serverPort);
    EXPECT_EQ(report.sessionDescriptor.headless, options.headless);
    EXPECT_EQ(report.sessionDescriptor.enableStartupUi, options.enableStartupUi);
    EXPECT_EQ(report.sessionDescriptor.startupContentRoot,
              options.startupContentRoot);
    EXPECT_EQ(report.sessionDescriptor.packageBaseDirectory,
              options.packageBaseDirectory);
    EXPECT_EQ(report.sessionDescriptor.domain, options.expectedDomain);
}

TEST(RuntimeStartupSessionTests, ValidPackageSucceedsThroughSessionFacade)
{
    const auto root = TempRoot("saga_runtime_session_valid_client");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(root);

    RuntimeLaunchOptions options;
    options.packageManifestPath = packagePath;

    const auto report = RuntimeStartupSession::Prepare(options);

    EXPECT_TRUE(report.Succeeded());
    EXPECT_FALSE(report.preflight.validationSkipped);
    EXPECT_EQ(report.sessionDescriptor.packageManifestPath, packagePath);
}

TEST(RuntimeStartupSessionTests, MissingPackageFailsWhenDevSkipDisabled)
{
    RuntimeLaunchOptions options;
    options.allowMissingPackageManifestForDev = false;

    const auto report = RuntimeStartupSession::Prepare(options);

    EXPECT_FALSE(report.Succeeded());
    EXPECT_FALSE(report.preflight.validationSkipped);
    ASSERT_EQ(report.preflight.diagnostics.size(), 1U);
}

TEST(RuntimeStartupSessionTests, ServerPackageSucceedsForServerDomain)
{
    const auto root = TempRoot("saga_runtime_session_valid_server");
    const auto packagePath = root / "package.json";
    WriteFile(packagePath, PackageJson("server"));
    WriteValidReferencedManifests(root);

    RuntimeLaunchOptions options;
    options.packageManifestPath = packagePath;
    options.expectedDomain = RuntimeStartupDomain::Server;

    const auto report = RuntimeStartupSession::Prepare(options);

    EXPECT_TRUE(report.Succeeded());
    EXPECT_EQ(report.sessionDescriptor.domain, RuntimeStartupDomain::Server);
}

TEST(RuntimeStartupSessionTests, PackageBaseDirectoryFlowsIntoPreflight)
{
    const auto root = TempRoot("saga_runtime_session_explicit_base");
    const auto packageRoot = root / "Package";
    const auto packageBase = root / "Base";
    const auto packagePath = packageRoot / "package.json";
    WriteFile(packagePath, PackageJson("client"));
    WriteValidReferencedManifests(packageBase);

    RuntimeLaunchOptions options;
    options.packageManifestPath = packagePath;
    options.packageBaseDirectory = packageBase;

    const auto report = RuntimeStartupSession::Prepare(options);

    EXPECT_TRUE(report.Succeeded());
    EXPECT_FALSE(report.preflight.validationSkipped);
    EXPECT_EQ(report.sessionDescriptor.packageManifestPath, packagePath);
    EXPECT_EQ(report.sessionDescriptor.packageBaseDirectory, packageBase);
}
