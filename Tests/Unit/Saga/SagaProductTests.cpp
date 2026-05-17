/// @file SagaProductTests.cpp
/// @brief Tests for Saga product orchestration startup boundaries.

#include "SagaAppConfig.h"
#include "SagaApp.h"
#include "SagaProjectSystem.h"
#include "SagaProductHost.h"
#include "SagaSdeCompiler.h"
#include "SagaWorkspaceResolver.h"

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <gtest/gtest.h>

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
