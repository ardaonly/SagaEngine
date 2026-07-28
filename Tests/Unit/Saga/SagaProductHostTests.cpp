// SPDX-License-Identifier: MPL-2.0
#include "SagaProductTestSupport.hpp"

using namespace SagaProduct;
using namespace SagaProductTestSupport;

namespace
{
TEST(SagaProductHostTest, PreparesRoleTargetsAtBoundaryLevel)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInProductWorkspaceRoot();
    workspace.runtimeRole = "SagaRuntime";
    workspace.serverRole.clear();

    SagaProductHost host;
    SagaSessionModel session;
    session.workspace = workspace;

    session.target = SagaProductTargetKind::Editor;
    SagaPreparedTarget editor = host.PrepareTarget(session);
    EXPECT_TRUE(editor.executableName.empty());
    EXPECT_EQ(editor.availability, SagaLaunchTargetAvailability::Unsupported);
    EXPECT_TRUE(editor.arguments.empty());

    session.target = SagaProductTargetKind::Runtime;
    session.packageManifestPath = fs::path("Packages/dev-client/package.json");
    SagaPreparedTarget runtime = host.PrepareTarget(session);
    EXPECT_EQ(runtime.executableName, "SagaRuntime");
    EXPECT_EQ(runtime.availability, SagaLaunchTargetAvailability::Bounded);
    ASSERT_EQ(runtime.arguments.size(), 2u);
    EXPECT_EQ(runtime.arguments[0], "--package-manifest");
    EXPECT_EQ(runtime.arguments[1], "Packages/dev-client/package.json");

    session.target = SagaProductTargetKind::Server;
    session.packageManifestPath = fs::path("Packages/dev-server/package.json");
    SagaPreparedTarget server = host.PrepareTarget(session);
    EXPECT_TRUE(server.executableName.empty());
    EXPECT_TRUE(server.arguments.empty());
    EXPECT_EQ(server.availability, SagaLaunchTargetAvailability::Unsupported);
}

TEST(SagaProductHostTest, EditorTargetCannotBePreparedAsAnExternalProcess)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Editor;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInProductWorkspaceRoot();

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(out.str().empty());
    EXPECT_NE(err.str().find(SagaProductDiagnostics::EditorHostedInProcess),
              std::string::npos);
}

TEST(SagaProductHostTest, MissingPackageManifestProducesProductDiagnostic)
{
    SagaWorkspaceDefinition workspace;
    workspace.id = "builtin.basic";
    workspace.root = BuiltInProductWorkspaceRoot();
    workspace.runtimeRole = "SagaRuntime";
    workspace.serverRole.clear();

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
    EXPECT_EQ(diagnostic.stage, SagaProductDiagnosticStage::TargetPreparation);
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
    config.builtInWorkspaceRoot = BuiltInProductWorkspaceRoot();

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.PackageManifestMissing"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=runtime"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.stage=target_preparation"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=runtime target requires --package-manifest"),
              std::string::npos);
}

TEST(SagaProductHostTest, ServerPreparationIsExplicitlyUnsupported)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Server;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInProductWorkspaceRoot();
    config.packageManifestPath = fs::path("Packages/dev-server/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.ServerExecutionUnsupported"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=server"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.stage=target_preparation"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=Product dedicated-server execution is not implemented."),
              std::string::npos);
    EXPECT_EQ(error.find("SagaServer"), std::string::npos);
}

TEST(SagaProductHostTest, RuntimePrepareOnlyOutputIncludesManifestArgument)
{
    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.prepareOnly = true;
    config.target = SagaProductTargetKind::Runtime;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInProductWorkspaceRoot();
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

TEST(SagaProductHostTest, ServerTargetNeverCreatesProcessLaunchRequest)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Server);
    config.packageManifestPath = fs::path("Packages/dev-server/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(launcherPtr->requests.empty());
    EXPECT_TRUE(out.str().empty());
    EXPECT_NE(err.str().find("Saga.Target.ServerExecutionUnsupported"),
              std::string::npos);
}

TEST(SagaProductHostTest, FutureServerMetadataDoesNotBecomeLaunchInstruction)
{
    const fs::path root = MakeTempDir("saga_future_server_metadata_test");
    WriteFile(root / "Server.sagaproj", R"({
        "schemaVersion": 0,
        "projectId": "future-server-metadata",
        "displayName": "Future Server Metadata",
        "serverRole": "future.dedicated.metadata"
    })");

    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Server);
    config.workspaceSelector = root.string();
    config.packageManifestPath = fs::path("Packages/future-server/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(launcherPtr->requests.empty());
    EXPECT_TRUE(out.str().empty());
    EXPECT_NE(err.str().find("Saga.Target.ServerExecutionUnsupported"),
              std::string::npos);
    EXPECT_EQ(err.str().find("future.dedicated.metadata"), std::string::npos);
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
    EXPECT_NE(error.find("diagnostic.stage=startup_handoff"), std::string::npos);
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
        SagaProductTargetKind::Runtime,
        SagaProductDiagnostics::ProcessExitedWithFailure,
        "runtime target process exited with code 7",
        fs::temp_directory_path() / "saga_bin" / "SagaRuntime"));
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Runtime);
    config.packageManifestPath = fs::path("Packages/dev-client/package.json");

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 7);
    ASSERT_EQ(launcherPtr->requests.size(), 1u);
    EXPECT_NE(out.str().find("launch.exitCode=7"), std::string::npos);
    const std::string error = err.str();
    EXPECT_NE(error.find("diagnostic.id=Saga.Target.ProcessExitedWithFailure"),
              std::string::npos);
    EXPECT_NE(error.find("diagnostic.target=runtime"), std::string::npos);
    EXPECT_NE(error.find("diagnostic.message=runtime target process exited with code 7"),
              std::string::npos);
}

TEST(SagaProductHostTest, EditorTargetNeverUsesExternalProcessLauncher)
{
    auto launcher = std::make_unique<FakeProcessLauncher>();
    FakeProcessLauncher* launcherPtr = launcher.get();
    launcherPtr->result.ok = true;
    launcherPtr->result.started = true;
    launcherPtr->result.exitCode = 0;
    SagaProduct::SagaApp app(std::move(launcher));

    SagaAppConfig config = MakeBasicTargetConfig(SagaProductTargetKind::Editor);

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(launcherPtr->requests.empty());
    EXPECT_TRUE(out.str().empty());
    EXPECT_NE(err.str().find(SagaProductDiagnostics::EditorHostedInProcess),
              std::string::npos);
}

} // namespace
