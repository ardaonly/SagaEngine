// SPDX-License-Identifier: MPL-2.0
#include "SagaProductTestSupport.hpp"

using namespace SagaProduct;
using namespace SagaProductTestSupport;

namespace
{
fs::path ManagedToolStage(const fs::path& root)
{
    const auto stage = root / "ManagedToolRuntime";
    const auto host = stage / "runtime/dotnet/dotnet";
    WriteFile(host, "#!/bin/sh\nexit 0\n");
    fs::permissions(host, fs::perms::owner_read | fs::perms::owner_write |
        fs::perms::owner_exec, fs::perm_options::replace);
    fs::create_directories(stage /
        "runtime/dotnet/shared/Microsoft.NETCore.App/10.0.10");
    fs::create_directories(stage / "runtime/dotnet/host/fxr/10.0.10");
    WriteFile(stage / "managed-tool-runtime.json", R"({
      "schemaVersion":1,"framework":"Microsoft.NETCore.App",
      "targetFramework":"net10.0","minimumRuntimeVersion":"10.0.0",
      "runtimeVersion":"10.0.10","rid":"linux-x64",
      "tools":[
        {"id":"ProjectKit","directory":"SagaProjectKit","assembly":"sagaproject"},
        {"id":"Script","directory":"SagaScript","assembly":"sagascript"},
        {"id":"Packager","directory":"SagaPackager","assembly":"sagapack"}
      ]})");
    const auto tool = stage / "tools/SagaScript/sagascript";
    WriteFile(tool.string() + ".dll", "fixture\n");
    WriteFile(tool.string() + ".deps.json", "{}\n");
    WriteFile(tool.string() + ".runtimeconfig.json",
        R"({"runtimeOptions":{"tfm":"net10.0","framework":{"name":"Microsoft.NETCore.App","version":"10.0.0"}}})");
    return stage;
}

TEST(SagaScriptGateTest, MissingScriptsDirectoryProducesProductDiagnostic)
{
    const fs::path root = MakeTempDir("saga_sagascript_missing_scripts_test");
    const fs::path manifest =
        WriteProject(root / "Project", "missing-scripts", "Missing Scripts");

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectManifest = manifest;

    std::ostringstream out;
    std::ostringstream err;
    const SagaScriptGateResult result =
        gate.ValidateProject(request, out, err);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.started);
    EXPECT_TRUE(runnerPtr->requests.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].stage,
              SagaProductDiagnosticStage::ProjectValidation);
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
    const fs::path manifest =
        WriteProject(root / "Project", "missing-scripts", "Missing Scripts");

    SagaProduct::SagaApp app;
    SagaAppConfig config;
    config.validateSagaScript = true;
    config.sagaScriptProjectManifest = manifest;

    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = app.Run(config, out, err);

    EXPECT_EQ(exitCode, 1);
    EXPECT_NE(err.str().find("diagnostic.id=Saga.Project.SagaScript.SourceMissing"),
              std::string::npos);
    EXPECT_NE(err.str().find("diagnostic.stage=project_validation"),
              std::string::npos);
    EXPECT_NE(out.str().find("sagascript.source="), std::string::npos);
    EXPECT_EQ(out.str().find("target="), std::string::npos);
}

TEST(SagaScriptGateTest, BuildsForgeGateRunCommandForProjectPaths)
{
    const fs::path root = MakeTempDir("saga_sagascript_gate_command_test");
    const fs::path projectRoot = root / "ScriptProject";
    const fs::path manifest =
        WriteProject(projectRoot, "script-project", "ScriptProject", true);

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    runnerPtr->result.started = true;
    runnerPtr->result.exitCode = 0;
    runnerPtr->result.standardOutput = "gate ok\n";
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectManifest = manifest;
    request.forgeExecutable = fs::path("/tools/forge");
    request.managedRuntime.testStage = ManagedToolStage(root);
    request.runtimeBridge = root / "SagaScript.RuntimeBridge.dll";

    std::ostringstream out;
    std::ostringstream err;
    const SagaScriptGateResult result =
        gate.ValidateProject(request, out, err);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(runnerPtr->requests.size(), 1u);
    const SagaToolProcessRequest& process = runnerPtr->requests[0];
    EXPECT_EQ(process.executablePath, fs::path("/tools/forge"));
    EXPECT_EQ(process.workingDirectory, fs::weakly_canonical(projectRoot));
    const std::vector<std::string> expected = {
        "gate",
        "run",
        "--name",
        "sagascript",
        "--tool",
        (request.managedRuntime.testStage / "runtime/dotnet/dotnet").string(),
        "--diagnostics",
        result.paths.diagnosticsOutputPath.string(),
        "--",
        (request.managedRuntime.testStage /
            "tools/SagaScript/sagascript.dll").string(),
        "compile",
        "--source",
        result.paths.sourceRoot.string(),
        "--out",
        result.paths.manifestOutputDirectory.string(),
        "--artifacts-out",
        result.paths.artifactOutputDirectory.string(),
        "--project-root",
        fs::weakly_canonical(projectRoot).string(),
        "--diagnostics",
        result.paths.diagnosticsOutputPath.string(),
    };
    EXPECT_EQ(process.arguments, expected);
    EXPECT_EQ(process.environment.at("DOTNET_ROOT"),
              (request.managedRuntime.testStage / "runtime/dotnet").string());
    EXPECT_EQ(process.environment.at("DOTNET_MULTILEVEL_LOOKUP"), "0");
    EXPECT_EQ(process.environment.at("SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"),
              request.runtimeBridge.string());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_NE(out.str().find("gate ok"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

TEST(SagaScriptGateTest, NonZeroForgeGateExitFailsValidation)
{
    const fs::path root = MakeTempDir("saga_sagascript_gate_failure_test");
    const fs::path manifest = WriteProject(root / "FailingScriptProject",
                                           "failing-script-project",
                                           "FailingScriptProject",
                                           true);

    auto runner = std::make_unique<FakeToolProcessRunner>();
    FakeToolProcessRunner* runnerPtr = runner.get();
    runnerPtr->result.started = true;
    runnerPtr->result.exitCode = 3;
    runnerPtr->result.standardError = "blocking diagnostics\n";
    SagaScriptGate gate(std::move(runner));

    SagaScriptGateRequest request;
    request.projectManifest = manifest;
    request.managedRuntime.testStage = ManagedToolStage(root);

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

} // namespace
