/// @file SagaProductTests.cpp
/// @brief Tests for Saga product orchestration startup boundaries.

#include "SagaAppConfig.h"
#include "SagaApp.h"
#include "SagaPackageStaging.h"
#include "SagaProjectSystem.h"
#include "SagaProductHost.h"
#include "SagaProductWorkflowSmokeReport.h"
#include "SagaPublishReadiness.h"
#include "SagaScriptGate.h"
#include "SagaSdeCompiler.h"
#include "SagaWorkspaceResolver.h"

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <SagaEngine/Packages/PackageManifestLoader.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

namespace fs = std::filesystem;
using namespace SagaProduct;

[[nodiscard]] fs::path SourceRoot()
{
    return fs::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] fs::path BuiltInBasicWorkspaceRoot()
{
    return SourceRoot() / "Apps" / "Saga" / "Definitions" / "BasicWorkspace";
}

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

void WriteValidPackageManifest(const fs::path& projectRoot,
                               const std::string& filename,
                               const std::string& packageId,
                               const std::string& packageKind)
{
    WriteFile(projectRoot / "Build" / "Manifests" / filename, R"({
  "schemaVersion": 1,
  "packageId": ")" + packageId + R"(",
  "packageKind": ")" + packageKind + R"(",
  "buildProfile": "shipping-full",
  "targetPlatform": "linux",
  "runtimeCompatibilityVersion": "0.0.8",
  "assetManifests": [
    { "id": "assets.main", "path": "Build/Manifests/assets.json" }
  ],
  "artifactManifests": [
    { "id": "artifacts.main", "path": "Build/Manifests/artifacts.json" }
  ],
  "packageHash": "sha256-test"
})");
}

void WriteValidPublishPackageInputs(const fs::path& projectRoot)
{
    WriteFile(projectRoot / "Build" / "Manifests" / "assets.json", "{}");
    WriteFile(projectRoot / "Build" / "Manifests" / "artifacts.json", "{}");
    WriteValidPackageManifest(
        projectRoot,
        "package_manifest.client.json",
        "starter.client",
        "client");
    WriteValidPackageManifest(
        projectRoot,
        "package_manifest.server.json",
        "starter.server",
        "server");
}

class FakeProcessLauncher final : public ISagaProcessLauncher
{
public:
    SagaProcessLaunchResult result;
    std::vector<SagaProcessLaunchRequest> requests;

    SagaProcessLaunchResult Launch(const SagaProcessLaunchRequest& request,
                                   std::ostream& out,
                                   std::ostream& err) override
    {
        requests.push_back(request);
        out << childStdout;
        err << childStderr;
        return result;
    }

    std::string childStdout;
    std::string childStderr;
};

class FakeToolProcessRunner final : public ISagaToolProcessRunner
{
public:
    SagaToolProcessResult result;
    std::vector<SagaToolProcessRequest> requests;

    SagaToolProcessResult Run(const SagaToolProcessRequest& request) override
    {
        requests.push_back(request);
        return result;
    }
};

[[nodiscard]] SagaAppConfig MakeBasicTargetConfig(SagaProductTargetKind target)
{
    SagaAppConfig config;
    config.target = target;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInBasicWorkspaceRoot();
    config.executablePath = fs::temp_directory_path() / "saga_bin" / "Saga";
    return config;
}

[[nodiscard]] SagaProductDiagnostic MakeTestLaunchDiagnostic(
    SagaProductTargetKind target,
    const char* diagnosticId,
    std::string message,
    fs::path path)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = target;
    diagnostic.phase = SagaProductDiagnosticPhase::StartupHandoff;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] const nlohmann::json* FindWorkflowStep(
    const nlohmann::json& report,
    const std::string& stepId)
{
    if (!report.contains("workflowSteps") || !report["workflowSteps"].is_array())
    {
        return nullptr;
    }
    for (const nlohmann::json& step : report["workflowSteps"])
    {
        if (step.value("id", std::string{}) == stepId)
        {
            return &step;
        }
    }
    return nullptr;
}

[[nodiscard]] bool JsonArrayContainsString(const nlohmann::json& array,
                                           const std::string& value)
{
    if (!array.is_array())
    {
        return false;
    }
    return std::any_of(array.begin(), array.end(),
                       [&](const nlohmann::json& item)
                       {
                           return item.is_string() &&
                               item.get<std::string>() == value;
                       });
}

TEST(SagaAppConfigTest, DefaultConfigCanBeCreated)
{
    const char* argvRaw[] = { "Saga" };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(1, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.workspaceSelector, "builtin:basic");
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaAppConfigTest, QtLaunchArgumentsArePassedThrough)
{
    const char* argvRaw[] = {
        "Saga",
        "--platform",
        "offscreen",
        "--style=Fusion",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(4, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Editor);
    EXPECT_FALSE(result.config.prepareOnly);
}

TEST(SagaAppConfigTest, PackageManifestArgumentIsParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--target",
        "runtime",
        "--package-manifest",
        "Packages/dev-client/package.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(5, argv);

    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_TRUE(result.config.packageManifestPath.has_value());
    EXPECT_EQ(*result.config.packageManifestPath,
              fs::path("Packages/dev-client/package.json"));
    EXPECT_EQ(result.config.target, SagaProductTargetKind::Runtime);
}

TEST(SagaAppConfigTest, SagaScriptValidationArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--validate-sagascript",
        "/tmp/project",
        "--forge",
        "/tools/forge",
        "--sagascript-tool",
        "/tools/sagascript",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(7, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.validateSagaScript);
    EXPECT_EQ(result.config.sagaScriptProjectRoot, fs::path("/tmp/project"));
    EXPECT_EQ(result.config.forgeExecutable, fs::path("/tools/forge"));
    EXPECT_EQ(result.config.sagaScriptExecutable, fs::path("/tools/sagascript"));
}

TEST(SagaAppConfigTest, PublishReadinessArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--publish-check",
        "/tmp/project",
        "--publish-profile",
        "shipping-full",
        "--publish-report",
        "/tmp/project/Build/Reports/publish_report.json",
        "--publish-diagnostics",
        "qa=/tmp/project/Build/Reports/qa.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(9, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.publishCheck);
    EXPECT_EQ(result.config.publishProjectRoot, fs::path("/tmp/project"));
    EXPECT_EQ(result.config.publishProfile, "shipping-full");
    EXPECT_EQ(result.config.publishReportPath,
              fs::path("/tmp/project/Build/Reports/publish_report.json"));
    ASSERT_EQ(result.config.publishDiagnostics.size(), 1u);
    EXPECT_EQ(result.config.publishDiagnostics[0],
              "qa=/tmp/project/Build/Reports/qa.json");
}

TEST(SagaAppConfigTest, PackageStagingArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--stage-packages",
        "/tmp/project",
        "--package-profile",
        "shipping-full",
        "--target-platform",
        "linux-x64",
        "--runtime-compatibility",
        "0.0.8",
        "--package-report",
        "/tmp/project/Build/Reports/package_stage_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(11, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.stagePackages);
    EXPECT_EQ(result.config.packageStageProjectRoot, fs::path("/tmp/project"));
    EXPECT_EQ(result.config.packageProfile, "shipping-full");
    EXPECT_EQ(result.config.targetPlatform, "linux-x64");
    EXPECT_EQ(result.config.runtimeCompatibilityVersion, "0.0.8");
    EXPECT_EQ(result.config.packageStageReportPath,
              fs::path("/tmp/project/Build/Reports/package_stage_report.json"));
}

TEST(SagaAppConfigTest, WorkflowSmokeArgumentsAreParsed)
{
    const char* argvRaw[] = {
        "Saga",
        "--workflow-smoke",
        "--project",
        "samples/StarterArena/StarterArena.sagaproj",
        "--profile",
        "technical_preview",
        "--workflow-report-out",
        "/tmp/starter_arena_product_shell_workflow_report.json",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(8, argv);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.config.workflowSmoke);
    EXPECT_EQ(result.config.workflowProjectPath,
              fs::path("samples/StarterArena/StarterArena.sagaproj"));
    EXPECT_EQ(result.config.workflowProfile, "technical_preview");
    EXPECT_EQ(result.config.workflowReportPath,
              fs::path("/tmp/starter_arena_product_shell_workflow_report.json"));
}

TEST(SagaAppConfigTest, MissingPackageManifestValueFailsDeterministically)
{
    const char* argvRaw[] = {
        "Saga",
        "--package-manifest",
    };
    auto* argv = const_cast<char**>(argvRaw);

    const SagaConfigResult result = ParseSagaAppConfig(2, argv);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error, "Saga: --package-manifest requires a path");
}

TEST(SagaProductInfoTest, VersionInfoResolvesFromDistributionJson)
{
    const fs::path root = MakeTempDir("saga_product_info_test");
    const fs::path versionJson = root / "version.json";
    WriteFile(versionJson, R"({
        "productName": "SAGA",
        "distributionVersion": "0.0.8-test",
        "buildVersion": "0.0.8",
        "gitCommit": "abc123",
        "platform": "Linux",
        "binaries": [ "Saga" ]
    })");

    SagaAppConfig config;
    config.versionInfoPath = versionJson;

    std::string error;
    const SagaProductInfo info = LoadProductInfo(config, error);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(info.productName, "SAGA");
    EXPECT_EQ(info.distributionVersion, "0.0.8-test");
    EXPECT_EQ(info.buildVersion, "0.0.8");
    EXPECT_EQ(info.gitCommit, "abc123");
    EXPECT_EQ(info.binaries.size(), 1u);
    EXPECT_EQ(info.binaries.front(), "Saga");
}

TEST(SagaWorkspaceResolverTest, BuiltInBasicWorkspaceResolvesThroughSde)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = "builtin:basic";
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.workspace.id, "builtin.basic");
    EXPECT_EQ(result.workspace.editorProfile, "saga.profile.basic");
    EXPECT_EQ(result.workspace.runtimeRole, "SagaRuntime");
    EXPECT_EQ(result.workspace.serverRole, "SagaServer");
    EXPECT_FALSE(result.workspace.artifactHash.empty());
}

TEST(SagaWorkspaceResolverTest, MissingSdeFailsDeterministically)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = (MakeTempDir("saga_missing_sde_test") / "missing").string();
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(SagaWorkspaceResolverTest, InvalidSdeFailsDeterministically)
{
    const fs::path root = MakeTempDir("saga_invalid_sde_test");
    WriteFile(root / "source" / "workspace.sde", "sde version nope");

    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = root.string();
    request.builtInBasicRoot = BuiltInBasicWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("validation failed"), std::string::npos);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(),
                            result.diagnostics.end(),
                            [](const std::string& diagnostic)
                            {
                                return diagnostic.find("SDE_EXPECTED_TOKEN") !=
                                       std::string::npos;
                            }));
}

TEST(SagaProductHostTest, PreparesRoleTargetsAtBoundaryLevel)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInBasicWorkspaceRoot();
    workspace.editorProfile = "saga.profile.basic";
    workspace.runtimeRole = "SagaRuntime";
    workspace.serverRole = "SagaServer";

    SagaProductHost host;
    SagaSessionModel session;
    session.workspace = workspace;

    session.target = SagaProductTargetKind::Editor;
    SagaPreparedTarget editor = host.PrepareTarget(session);
    EXPECT_EQ(editor.executableName, "Saga");
    EXPECT_EQ(editor.moduleName, "SagaEditorModule");
    EXPECT_TRUE(editor.sameProcess);
    EXPECT_TRUE(editor.arguments.empty());

    session.target = SagaProductTargetKind::Runtime;
    session.packageManifestPath = fs::path("Packages/dev-client/package.json");
    SagaPreparedTarget runtime = host.PrepareTarget(session);
    EXPECT_EQ(runtime.executableName, "SagaRuntime");
    EXPECT_EQ(runtime.moduleName, "SagaRuntimeModule");
    EXPECT_FALSE(runtime.sameProcess);
    ASSERT_EQ(runtime.arguments.size(), 2u);
    EXPECT_EQ(runtime.arguments[0], "--package-manifest");
    EXPECT_EQ(runtime.arguments[1], "Packages/dev-client/package.json");

    session.target = SagaProductTargetKind::Server;
    session.packageManifestPath = fs::path("Packages/dev-server/package.json");
    SagaPreparedTarget server = host.PrepareTarget(session);
    EXPECT_EQ(server.executableName, "SagaServer");
    EXPECT_EQ(server.moduleName, "SagaServerModule");
    EXPECT_FALSE(server.sameProcess);
    ASSERT_EQ(server.arguments.size(), 2u);
    EXPECT_EQ(server.arguments[0], "--package-manifest");
    EXPECT_EQ(server.arguments[1], "Packages/dev-server/package.json");
}

TEST(SagaProductHostTest, EditorTargetDoesNotRequirePackageManifest)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Editor;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInBasicWorkspaceRoot();

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("target=editor"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProductHostTest, MissingPackageManifestProducesProductDiagnostic)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInBasicWorkspaceRoot();
    workspace.editorProfile = "saga.profile.basic";
    workspace.runtimeRole = "SagaRuntime";
    workspace.serverRole = "SagaServer";

    SagaProductHost host;
    SagaSessionModel session;
    session.workspace = workspace;
    session.target = SagaProductTargetKind::Runtime;

    const SagaTargetPreparationResult result =
        host.PrepareTargetWithDiagnostics(session);

    ASSERT_FALSE(result.ok);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    const SagaProductDiagnostic& diagnostic = result.diagnostics[0];
    EXPECT_EQ(diagnostic.target, SagaProductTargetKind::Runtime);
    EXPECT_EQ(diagnostic.phase, SagaProductDiagnosticPhase::TargetPreparation);
    EXPECT_EQ(diagnostic.diagnosticId,
              SagaProductDiagnostics::PackageManifestMissing);
    EXPECT_EQ(diagnostic.message, "runtime target requires --package-manifest");
    EXPECT_FALSE(diagnostic.path.has_value());
}

TEST(SagaProductHostTest, RuntimePrepareRequiresPackageManifest)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Runtime;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInBasicWorkspaceRoot();

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.PackageManifestMissing"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=runtime"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.phase=target_preparation"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=runtime target requires --package-manifest"),
              std::string::npos);
}

TEST(SagaProductHostTest, ServerPrepareRequiresPackageManifest)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Server;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInBasicWorkspaceRoot();

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.PackageManifestMissing"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=server"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.phase=target_preparation"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=server target requires --package-manifest"),
              std::string::npos);
}

TEST(SagaProductHostTest, RuntimePrepareOnlyOutputIncludesManifestArgument)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Runtime;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInBasicWorkspaceRoot();
    config.packageManifestPath = fs::path("Packages/dev-client/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    const std::string output = out.str();
    EXPECT_NE(output.find("target=runtime"), std::string::npos);
    EXPECT_NE(output.find("argument=--package-manifest"), std::string::npos);
    EXPECT_NE(output.find("argument=Packages/dev-client/package.json"),
              std::string::npos);
}

TEST(SagaProductHostTest, RuntimeTargetLaunchesWhenNotPrepareOnly)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    launcherPtr->result.ok = true;
    launcherPtr->result.started = true;
    launcherPtr->result.exitCode = 0;
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Runtime);
    config.packageManifestPath = fs::path("Packages/dev-client/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    ASSERT_EQ(launcherPtr->requests.size(), 1u);
    const SagaProcessLaunchRequest& request = launcherPtr->requests[0];
    EXPECT_EQ(request.target, SagaProductTargetKind::Runtime);
    EXPECT_EQ(request.executablePath,
              config.executablePath.parent_path() / "SagaRuntime");
    ASSERT_EQ(request.arguments.size(), 2u);
    EXPECT_EQ(request.arguments[0], "--package-manifest");
    EXPECT_EQ(request.arguments[1], "Packages/dev-client/package.json");
    EXPECT_NE(out.str().find("launch.exitCode=0"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProductHostTest, ServerTargetLaunchesWhenNotPrepareOnly)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    launcherPtr->result.ok = true;
    launcherPtr->result.started = true;
    launcherPtr->result.exitCode = 0;
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Server);
    config.packageManifestPath = fs::path("Packages/dev-server/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    ASSERT_EQ(launcherPtr->requests.size(), 1u);
    const SagaProcessLaunchRequest& request = launcherPtr->requests[0];
    EXPECT_EQ(request.target, SagaProductTargetKind::Server);
    EXPECT_EQ(request.executablePath,
              config.executablePath.parent_path() / "SagaServer");
    ASSERT_EQ(request.arguments.size(), 2u);
    EXPECT_EQ(request.arguments[0], "--package-manifest");
    EXPECT_EQ(request.arguments[1], "Packages/dev-server/package.json");
    EXPECT_NE(out.str().find("launch.exitCode=0"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProductHostTest, PrepareOnlyDoesNotLaunchRuntimeTarget)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Runtime);
    config.prepareOnly = true;
    config.packageManifestPath = fs::path("Packages/dev-client/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(launcherPtr->requests.empty());
    EXPECT_NE(out.str().find("target=runtime"), std::string::npos);
    EXPECT_EQ(out.str().find("launch.exitCode="), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProductHostTest, LaunchStartFailureIsReportedDeterministically)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    launcherPtr->result.started = false;
    launcherPtr->result.exitCode = -1;
    launcherPtr->result.diagnostics.push_back(MakeTestLaunchDiagnostic(
        SagaProductTargetKind::Runtime,
        SagaProductDiagnostics::ProcessStartFailed,
        "runtime target process failed to start: test failure",
        fs::temp_directory_path() / "saga_bin" / "SagaRuntime"));
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Runtime);
    config.packageManifestPath = fs::path("Packages/dev-client/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    ASSERT_EQ(launcherPtr->requests.size(), 1u);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.ProcessStartFailed"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=runtime"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.phase=startup_handoff"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=runtime target process failed to start: test failure"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.path="), std::string::npos);
    EXPECT_EQ(out.str().find("launch.exitCode="), std::string::npos);
}

TEST(SagaProductHostTest, NonZeroLaunchExitCodeIsReportedAndReturned)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    launcherPtr->result.started = true;
    launcherPtr->result.exitCode = 7;
    launcherPtr->result.diagnostics.push_back(MakeTestLaunchDiagnostic(
        SagaProductTargetKind::Server,
        SagaProductDiagnostics::ProcessExitedWithFailure,
        "server target process exited with code 7",
        fs::temp_directory_path() / "saga_bin" / "SagaServer"));
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Server);
    config.packageManifestPath = fs::path("Packages/dev-server/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 7);
    ASSERT_EQ(launcherPtr->requests.size(), 1u);
    EXPECT_NE(out.str().find("launch.exitCode=7"), std::string::npos);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.ProcessExitedWithFailure"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=server"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=server target process exited with code 7"),
              std::string::npos);
}

TEST(SagaProductHostTest, EditorTargetDoesNotUseProcessLauncher)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Editor);

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(launcherPtr->requests.empty());
    EXPECT_NE(out.str().find("target=editor"), std::string::npos);
    EXPECT_EQ(out.str().find("launch.exitCode="), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProjectSystemTest, CreateOpenAndRememberProjectAreReal)
{
    const fs::path root = MakeTempDir("saga_project_system_test");
    SagaProjectSystem projects(root / "recent_projects.json");

    const SagaProjectResult created =
        projects.CreateProject(root, "FirstProject");

    ASSERT_TRUE(created.ok) << created.error;
    EXPECT_TRUE(fs::exists(created.manifest.root / "saga.project.json"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "source" / "workspace.sde"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "source" / "editor" / "profiles.sde"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "artifacts"));
    EXPECT_TRUE(fs::exists(created.manifest.root / ".sde" / "cache"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Assets"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Scripts"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Generated"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Build"));
    EXPECT_TRUE(fs::exists(created.manifest.root / "Packages"));
    EXPECT_EQ(created.manifest.sdeRoot, created.manifest.root / ".sde");

    const SagaProjectResult opened = projects.OpenProject(created.manifest.root);
    ASSERT_TRUE(opened.ok) << opened.error;
    EXPECT_EQ(opened.manifest.displayName, "FirstProject");
    EXPECT_EQ(opened.manifest.sdeRoot, opened.manifest.root / ".sde");

    const std::vector<SagaRecentProject> recent = projects.LoadRecentProjects();
    ASSERT_FALSE(recent.empty());
    EXPECT_EQ(recent.front().displayName, "FirstProject");
    EXPECT_TRUE(recent.front().exists);
}

TEST(SagaScriptGateTest, MissingScriptsDirectoryProducesProductDiagnostic)
{
    const fs::path root = MakeTempDir("saga_sagascript_missing_scripts_test");
    WriteFile(root / "Project" / "saga.project.json", R"({
        "schemaVersion": 1,
        "projectId": "missing-scripts",
        "displayName": "Missing Scripts",
        "sdeRoot": ".sde"
    })");

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectRoot = root / "Project";

    std::ostringstream out;
    std::ostringstream err;
    const SagaScriptGateResult result =
        gate.ValidateProject(request, out, err);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.started);
    EXPECT_TRUE(runnerPtr->requests.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].phase,
              SagaProductDiagnosticPhase::ProjectValidation);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaProductDiagnostics::SagaScriptSourceMissing);
    EXPECT_NE(result.diagnostics[0].message.find("Scripts"),
              std::string::npos);
    EXPECT_TRUE(out.str().empty());
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaScriptGateTest, AppEntrypointReportsMissingScriptsDirectory)
{
    const fs::path root = MakeTempDir("saga_sagascript_app_entrypoint_test");
    WriteFile(root / "Project" / "saga.project.json", R"({
        "schemaVersion": 1,
        "projectId": "missing-scripts",
        "displayName": "Missing Scripts",
        "sdeRoot": ".sde"
    })");

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.validateSagaScript = true;
    config.sagaScriptProjectRoot = root / "Project";

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_NE(err.str().find("diagnostic.id=Saga.Project.SagaScript.SourceMissing"),
              std::string::npos);
    EXPECT_NE(err.str().find("diagnostic.phase=project_validation"),
              std::string::npos);
    EXPECT_NE(out.str().find("sagascript.source="), std::string::npos);
    EXPECT_EQ(out.str().find("target="), std::string::npos);
}

TEST(SagaScriptGateTest, BuildsForgeGateRunCommandForProjectPaths)
{
    const fs::path root = MakeTempDir("saga_sagascript_gate_command_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "ScriptProject");
    ASSERT_TRUE(created.ok) << created.error;

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    runnerPtr->result.started = true;
    runnerPtr->result.exitCode = 0;
    runnerPtr->result.standardOutput = "gate ok\n";
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectRoot = created.manifest.root;
    request.forgeExecutable = fs::path("/tools/forge");
    request.sagaScriptExecutable = fs::path("/tools/sagascript");

    std::ostringstream out;
    std::ostringstream err;
    const SagaScriptGateResult result =
        gate.ValidateProject(request, out, err);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(runnerPtr->requests.size(), 1u);
    const SagaToolProcessRequest& process = runnerPtr->requests[0];
    EXPECT_EQ(process.executablePath, fs::path("/tools/forge"));
    EXPECT_EQ(process.workingDirectory, created.manifest.root);
    const std::vector<std::string> expected = {
        "gate",
        "run",
        "--name",
        "sagascript",
        "--tool",
        "/tools/sagascript",
        "--diagnostics",
        result.paths.diagnosticsOutputPath.string(),
        "--",
        "compile",
        "--source",
        result.paths.sourceRoot.string(),
        "--out",
        result.paths.manifestOutputDirectory.string(),
        "--artifacts-out",
        result.paths.artifactOutputDirectory.string(),
        "--project-root",
        created.manifest.root.string(),
        "--diagnostics",
        result.paths.diagnosticsOutputPath.string(),
    };
    EXPECT_EQ(process.arguments, expected);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_NE(out.str().find("gate ok"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaScriptGateTest, NonZeroForgeGateExitFailsValidation)
{
    const fs::path root = MakeTempDir("saga_sagascript_gate_failure_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "FailingScriptProject");
    ASSERT_TRUE(created.ok) << created.error;

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    runnerPtr->result.started = true;
    runnerPtr->result.exitCode = 3;
    runnerPtr->result.standardError = "blocking diagnostics\n";
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectRoot = created.manifest.root;

    std::ostringstream out;
    std::ostringstream err;
    const SagaScriptGateResult result =
        gate.ValidateProject(request, out, err);

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.started);
    EXPECT_EQ(result.exitCode, 3);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              SagaProductDiagnostics::SagaScriptGateFailed);
    EXPECT_NE(err.str().find("blocking diagnostics"), std::string::npos);
}

TEST(SagaPackageStagingTest, StagesPackageManifestsForPublishReadiness)
{
    const fs::path root = MakeTempDir("saga_package_stage_test");
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
    const fs::path root = MakeTempDir("saga_package_stage_missing_inputs_test");
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
    const fs::path root = MakeTempDir("saga_package_stage_script_artifact_test");
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

TEST(SagaPackageStagingTest, AppEntrypointStagesPackages)
{
    const fs::path root = MakeTempDir("saga_package_stage_app_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PackageStageAppProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteFile(
        created.manifest.root / "Build" / "Manifests" / "assets.json",
        "{}");

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.stagePackages = true;
    config.packageStageProjectRoot = created.manifest.root;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("package.status=staged"), std::string::npos);
    EXPECT_NE(out.str().find("package.clientManifest="), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProductWorkflowSmokeTest,
     AppEntrypointWritesStarterArenaReportWithoutRunningWorkflowTools)
{
    const fs::path root = MakeTempDir("saga_product_workflow_smoke_test");
    const fs::path reportPath =
        root / "starter_arena_product_shell_workflow_report.json";

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.workflowSmoke = true;
    config.workflowProjectPath = SourceRoot() / "samples" / "StarterArena" /
        "StarterArena.sagaproj";
    config.workflowProfile = "technical_preview";
    config.workflowReportPath = reportPath;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("workflow.report="), std::string::npos);
    EXPECT_NE(out.str().find("workflow.status=PassedWithLimitations"),
              std::string::npos);
    EXPECT_TRUE(err.str().empty());
    ASSERT_TRUE(fs::exists(reportPath));

    std::ifstream input(reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["tool"], "Saga");
    EXPECT_EQ(report["command"], "workflow-smoke");
    EXPECT_EQ(report["verified"], false);
    EXPECT_EQ(report["status"], "PassedWithLimitations");
    EXPECT_EQ(report["project"]["projectId"], "starter-arena");
    EXPECT_EQ(report["profile"]["requestedProfileId"], "technical_preview");

    const nlohmann::json* editorInspection =
        FindWorkflowStep(report, "editor_inspection");
    ASSERT_NE(editorInspection, nullptr);
    EXPECT_NE((*editorInspection)["command"].get<std::string>().find(
                  "SagaEditor --inspect-project"),
              std::string::npos);

    const nlohmann::json* packagePreflight =
        FindWorkflowStep(report, "package_preflight");
    ASSERT_NE(packagePreflight, nullptr);
    EXPECT_EQ((*packagePreflight)["status"], "blocked");
    EXPECT_EQ((*packagePreflight)["command"], "scripts/package-linux-saga");

    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "full product dashboard"));
    EXPECT_TRUE(JsonArrayContainsString(report["nonClaims"],
                                        "distribution readiness"));
}

TEST(SagaPublishReadinessTest, ValidProjectAndPackagesAreReady)
{
    const fs::path root = MakeTempDir("saga_publish_ready_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PublishReadyProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPublishPackageInputs(created.manifest.root);

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;
    request.profile = "shipping-full";

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Ready);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    ASSERT_TRUE(fs::exists(result.reportPath));

    std::ifstream input(result.reportPath);
    const nlohmann::json report = nlohmann::json::parse(input);
    EXPECT_EQ(report["status"], "ready");
    EXPECT_EQ(report["blockers"].size(), 0u);
}

TEST(SagaPublishReadinessTest, MissingPackageManifestBlocksPublish)
{
    const fs::path root = MakeTempDir("saga_publish_missing_package_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PublishMissingPackageProject");
    ASSERT_TRUE(created.ok) << created.error;

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::Blocked);
    ASSERT_FALSE(result.report.readiness.blockers.empty());
    EXPECT_TRUE(std::any_of(result.report.readiness.blockers.begin(),
                            result.report.readiness.blockers.end(),
                            [](const SagaShared::Publish::PublishBlocker& blocker)
                            {
                                return blocker.kind ==
                                    SagaShared::Publish::PublishBlockerKind::
                                        PackageManifestInvalid;
                            }));
}

TEST(SagaPublishReadinessTest, BlockingDiagnosticsBlockPublish)
{
    const fs::path root = MakeTempDir("saga_publish_blocking_diagnostics_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PublishBlockingDiagnosticsProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPublishPackageInputs(created.manifest.root);
    const fs::path diagnostics =
        created.manifest.root / "Build" / "Reports" / "qa.json";
    WriteFile(diagnostics, R"({
  "summary": {
    "hasBlockingDiagnostics": true,
    "errorCount": 1
  }
})");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;
    request.diagnosticsInputs.push_back({"qa", diagnostics});

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_FALSE(result.ok);
    ASSERT_EQ(result.report.readiness.blockers.size(), 1u);
    EXPECT_EQ(result.report.readiness.blockers[0].kind,
              SagaShared::Publish::PublishBlockerKind::DiagnosticsFatal);
}

TEST(SagaPublishReadinessTest, WarningOnlyDiagnosticsProduceReadyWithWarnings)
{
    const fs::path root = MakeTempDir("saga_publish_warning_diagnostics_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PublishWarningDiagnosticsProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPublishPackageInputs(created.manifest.root);
    const fs::path diagnostics =
        created.manifest.root / "Build" / "Reports" / "qa.json";
    WriteFile(diagnostics, R"({
  "summary": {
    "hasBlockingDiagnostics": false,
    "warningCount": 2
  }
})");

    SagaPublishReadinessRequest request;
    request.projectRoot = created.manifest.root;
    request.diagnosticsInputs.push_back({"qa", diagnostics});

    SagaPublishReadinessService service;
    const SagaPublishReadinessResult result = service.Check(request);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.report.readiness.status,
              SagaShared::Publish::PublishStatus::ReadyWithWarnings);
    EXPECT_TRUE(result.report.readiness.blockers.empty());
    EXPECT_EQ(result.report.diagnosticSummary.warningCount, 1u);
}

TEST(SagaPublishReadinessTest, AppEntrypointWritesPublishReport)
{
    const fs::path root = MakeTempDir("saga_publish_app_entrypoint_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created =
        projects.CreateProject(root, "PublishAppEntrypointProject");
    ASSERT_TRUE(created.ok) << created.error;
    WriteValidPublishPackageInputs(created.manifest.root);

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.publishCheck = true;
    config.publishProjectRoot = created.manifest.root;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(out.str().find("publish.status=ready"), std::string::npos);
    EXPECT_NE(out.str().find("publish.blockers=0"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaProjectSystemTest, InvalidRoomCodesFailWithoutFakeJoin)
{
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("").has_value());
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("BAD").has_value());
    EXPECT_TRUE(SagaProjectSystem::ValidateRoomCode("ROOM 1234").has_value());
    EXPECT_FALSE(SagaProjectSystem::ValidateRoomCode("ROOM-1234").has_value());
}

TEST(SagaSdeCompilerTest, CompileUsesRealSdeAndDoesNotReplaceLastGoodOnFailure)
{
    const fs::path root = MakeTempDir("saga_sde_compiler_test");
    SagaProjectSystem projects(root / "recent_projects.json");
    const SagaProjectResult created = projects.CreateProject(root, "CompileProject");
    ASSERT_TRUE(created.ok) << created.error;

    SagaSdeCompiler compiler;
    const SagaSdeCompileResult good =
        compiler.Compile(created.manifest.sdeRoot);

    ASSERT_TRUE(good.sdeAvailable);
    ASSERT_TRUE(good.ok) << good.message;
    ASSERT_TRUE(good.hashes.has_value());
    EXPECT_FALSE(good.hashes->artifactHash.empty());
    ASSERT_TRUE(compiler.LastGoodHashes().has_value());

    WriteFile(created.manifest.sdeRoot / "source" / "Broken.sde", "sde version nope");
    const SagaSdeCompileResult bad =
        compiler.Compile(created.manifest.sdeRoot);

    EXPECT_FALSE(bad.ok);
    EXPECT_TRUE(compiler.LastGoodHashes().has_value());
    EXPECT_EQ(compiler.LastGoodHashes()->artifactHash, good.hashes->artifactHash);
}

TEST(SagaEditorBoundaryTest, EditorConsumesPreparedWorkspaceWithoutProductOrchestration)
{
    SagaEditor::EditorWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInBasicWorkspaceRoot();
    workspace.initialProfileId = "saga.profile.basic";
    workspace.sdeValidated = true;

    SagaEditor::EditorHost host;
    ASSERT_TRUE(host.Init(
        std::make_unique<SagaEditor::MemoryEditorSettingsStore>(),
        workspace));

    ASSERT_TRUE(host.GetWorkspaceDefinition().has_value());
    EXPECT_EQ(host.GetWorkspaceDefinition()->id, "builtin.basic");
    EXPECT_TRUE(host.GetWorkspaceDefinition()->sdeValidated);

    const fs::path editorMain = SourceRoot() / "Apps" / "Editor" / "main.cpp";
    std::ifstream input(editorMain);
    ASSERT_TRUE(input.is_open());
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());

    EXPECT_EQ(text.find("--host"), std::string::npos);
    EXPECT_EQ(text.find("--join"), std::string::npos);
    EXPECT_EQ(text.find("--server"), std::string::npos);
    EXPECT_EQ(text.find("SagaProduct"), std::string::npos);

    host.Shutdown();
}

} // namespace
