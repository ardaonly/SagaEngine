// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/RuntimeProcessController.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "TestSourceRoot.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <thread>

namespace
{
std::filesystem::path StarterArenaProject()
{
    return TestSourceRoot() /
        "Samples/StarterArena/StarterArena.sagaproj";
}

std::filesystem::path RuntimeExecutable()
{
    return std::filesystem::path(SAGA_BUILD_ROOT) / "bin/SagaRuntime";
}

bool Terminal(SagaEditor::RuntimePlayState state)
{
    return state == SagaEditor::RuntimePlayState::Completed ||
           state == SagaEditor::RuntimePlayState::Failed;
}

bool WaitForState(SagaEditor::RuntimeProcessController& controller,
                  SagaEditor::RuntimePlayState expected,
                  std::chrono::seconds timeout = std::chrono::seconds(15))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (controller.Snapshot().state == expected) return true;
        if (Terminal(controller.Snapshot().state)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

SagaEditor::RuntimeProcessSnapshot WaitForTerminal(
    SagaEditor::RuntimeProcessController& controller,
    std::chrono::seconds timeout = std::chrono::seconds(15))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!Terminal(controller.Snapshot().state) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return controller.Snapshot();
}

#ifndef _WIN32
std::filesystem::path RuntimeTestHelper(std::string_view mode)
{
    const auto root = std::filesystem::path(SAGA_BUILD_ROOT) /
        "runtime-controller-test-helpers";
    std::filesystem::create_directories(root);
    const auto path = root / (std::string(mode) + ".sh");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << R"(#!/bin/sh
PATH=/run/current-system/sw/bin:/usr/bin:/bin
envelope="$2"
json_value() {
    key="$1"
    while IFS= read -r line; do
        case "$line" in
            *\"$key\"*)
                value=${line#*: \"}
                value=${value%\",}
                value=${value%\"}
                printf '%s' "$value"
                return
                ;;
        esac
    done < "$envelope"
}
ready=$(json_value startupReadyPath)
session=$(json_value playSessionId)
manifest_hash=$(json_value canonicalManifestHash)
publish_ready() {
    temporary="${ready}.helper.tmp"
    printf '{"canonicalLaunchManifestHash":"%s","playSessionId":"%s","schemaVersion":1,"status":"Ready"}\n' "$manifest_hash" "$session" > "$temporary"
    mv "$temporary" "$ready"
}
)";
    output << "mode='" << mode << "'\n";
    output << R"(case "$mode" in
    ready-hang)
        publish_ready
        trap '' TERM
        while :; do sleep 1; done
        ;;
    hang-no-ready)
        trap '' TERM
        while :; do sleep 1; done
        ;;
    malformed-ready)
        printf 'not-json\n' > "$ready"
        while :; do sleep 1; done
        ;;
    wrong-session)
        printf '{"canonicalLaunchManifestHash":"%s","playSessionId":"old-session","schemaVersion":1,"status":"Ready"}\n' "$manifest_hash" > "$ready"
        while :; do sleep 1; done
        ;;
    wrong-manifest)
        printf '{"canonicalLaunchManifestHash":"sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","playSessionId":"%s","schemaVersion":1,"status":"Ready"}\n' "$session" > "$ready"
        while :; do sleep 1; done
        ;;
    ready-crash)
        publish_ready
        sleep 1
        kill -SEGV $$
        ;;
    exit-before-ready)
        exit 0
        ;;
esac
)";
    output.close();
    std::filesystem::permissions(path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
        std::filesystem::perms::owner_exec | std::filesystem::perms::group_read |
        std::filesystem::perms::group_exec,
        std::filesystem::perm_options::replace);
    return path;
}

SagaEditor::RuntimeProcessLaunchRequest HelperRequest(
    std::string session, std::string_view mode)
{
    SagaEditor::RuntimeProcessLaunchRequest request;
    request.playSessionId = std::move(session);
    request.runtimeExecutable = RuntimeTestHelper(mode);
    request.headless = true;
    request.startupTimeout = std::chrono::seconds(5);
    request.runtimeTimeout = std::chrono::seconds(5);
    request.gracefulStopTimeout = std::chrono::milliseconds(100);
    request.sigtermGrace = std::chrono::milliseconds(100);
    request.sigkillGrace = std::chrono::milliseconds(100);
    return request;
}
#endif
}

TEST(StarterArenaPlayStopIntegrationTests,
     SeparateRuntimeLoadsArtifactsAndReturnsClosedOpenClosedResult)
{
    std::error_code cleanupError;
    std::filesystem::remove_all(StarterArenaProject().parent_path() /
        "Build/Editor/Play/controller-headless-smoke", cleanupError);
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    SagaEditor::RuntimeProcessLaunchRequest request;
    request.playSessionId = "controller-headless-smoke";
    request.runtimeExecutable = RuntimeExecutable();
    request.headless = true;
    request.smokeInteractions = true;
    request.startupTimeout = std::chrono::seconds(30);
    request.runtimeTimeout = std::chrono::seconds(60);
    ASSERT_TRUE(controller.Start(workspace, request));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!Terminal(controller.Snapshot().state) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto snapshot = controller.Snapshot();
    controller.Shutdown();
    ASSERT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Completed)
        << snapshot.diagnosticCode << ": " << snapshot.message;
    ASSERT_TRUE(std::filesystem::is_regular_file(snapshot.resultPath));
    ASSERT_TRUE(snapshot.runtimeResult.has_value());
    EXPECT_TRUE(snapshot.runtimeResult->Successful());
    ASSERT_TRUE(std::filesystem::is_regular_file(snapshot.startupReadyPath));
    std::ifstream readyInput(snapshot.startupReadyPath, std::ios::binary);
    const std::string readyBytes{std::istreambuf_iterator<char>(readyInput),
                                 std::istreambuf_iterator<char>()};
    EXPECT_TRUE(SagaRuntime::Play::ParseRuntimeStartupReadyV1(
        readyBytes, request.playSessionId,
        snapshot.runtimeResult->canonicalLaunchManifestHash).Valid());
    std::ifstream input(snapshot.resultPath, std::ios::binary);
    const auto result = nlohmann::json::parse(input);
    EXPECT_EQ(result.value("status", std::string{}), "Succeeded");
    EXPECT_EQ(result.value("interactionActionCount", 0), 2);
    EXPECT_EQ(result["observedState"].value("transition", std::string{}),
              "ClosedOpenClosed");
    EXPECT_FALSE(std::filesystem::exists(snapshot.resultPath.parent_path() /
        "RuntimeScriptPackage/Manifests/script_artifacts.json"));
    EXPECT_FALSE(std::filesystem::exists(snapshot.resultPath.parent_path() /
        "RuntimeScriptPackage/Manifests/script_capabilities.json"));
}

TEST(StarterArenaGraphicalRuntimeIntegrationTests,
     SeparateRuntimeRendersAndReturnsClosedOpenClosedPixels)
{
    if (!std::getenv("SAGA_REQUIRE_REAL_RUNTIME_RENDER"))
        GTEST_SKIP() << "Set SAGA_REQUIRE_REAL_RUNTIME_RENDER=1 on a graphics-capable desktop.";
    std::error_code cleanupError;
    const auto sessionRoot = StarterArenaProject().parent_path() /
        "Build/Editor/Play/controller-graphical-smoke";
    std::filesystem::remove_all(sessionRoot, cleanupError);
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    SagaEditor::RuntimeProcessLaunchRequest request;
    request.playSessionId = "controller-graphical-smoke";
    request.runtimeExecutable = RuntimeExecutable();
    request.headless = false;
    request.smokeInteractions = true;
    request.startupTimeout = std::chrono::seconds(30);
    request.runtimeTimeout = std::chrono::seconds(60);
    ASSERT_TRUE(controller.Start(workspace, request));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!Terminal(controller.Snapshot().state) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto snapshot = controller.Snapshot();
    controller.Shutdown();
    ASSERT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Completed)
        << snapshot.diagnosticCode << ": " << snapshot.message;
    ASSERT_TRUE(snapshot.runtimeResult.has_value());
    ASSERT_TRUE(snapshot.runtimeResult->Successful());
    const auto observed = nlohmann::json::parse(
        snapshot.runtimeResult->observedStateCanonicalJson);
    EXPECT_EQ(observed.value("transition", std::string{}), "ClosedOpenClosed");
    EXPECT_TRUE(observed.value("meaningfulRendererPixels", false));
    EXPECT_GT(observed.value("drawCount", 0U), 0U);
    EXPECT_TRUE(std::filesystem::is_regular_file(sessionRoot / "runtime-frame.ppm"));
}

TEST(RuntimeProcessControllerTests, RejectsSecondConcurrentPlaySession)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    SagaEditor::RuntimeProcessLaunchRequest request;
    request.playSessionId = "one-active-session";
    request.runtimeExecutable = RuntimeExecutable();
    request.headless = true;
    request.smokeInteractions = true;
    ASSERT_TRUE(controller.Start(workspace, request));
    request.playSessionId = "forbidden-second-session";
    EXPECT_FALSE(controller.Start(workspace, request));
    controller.Stop();
    controller.Shutdown();
    EXPECT_TRUE(Terminal(controller.Snapshot().state));
}

TEST(RuntimeProcessControllerTests, WorkspaceDestructionAndRepeatedStopAreBoundedWhilePreparing)
{
    const auto startedAt = std::chrono::steady_clock::now();
    SagaEditor::RuntimeProcessController controller;
    {
        SagaEditor::EditorDocumentWorkspace workspace;
        ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
        SagaEditor::RuntimeProcessLaunchRequest request;
        request.playSessionId = "destroy-while-preparing";
        request.runtimeExecutable = RuntimeExecutable();
        request.headless = true;
        request.startupTimeout = std::chrono::seconds(10);
        request.runtimeTimeout = std::chrono::seconds(20);
        ASSERT_TRUE(controller.Start(workspace, request));
    }
    controller.Stop();
    controller.Stop();
    controller.Shutdown();

    EXPECT_TRUE(Terminal(controller.Snapshot().state));
    EXPECT_LT(std::chrono::steady_clock::now() - startedAt, std::chrono::seconds(15));
}

TEST(RuntimeProcessControllerTests, RunningSessionReturnsValidatedGracefulStopResult)
{
    std::error_code cleanupError;
    std::filesystem::remove_all(StarterArenaProject().parent_path() /
        "Build/Editor/Play/validated-graceful-stop", cleanupError);
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    SagaEditor::RuntimeProcessLaunchRequest request;
    request.playSessionId = "validated-graceful-stop";
    request.runtimeExecutable = RuntimeExecutable();
    request.headless = true;
    request.startupTimeout = std::chrono::seconds(15);
    request.runtimeTimeout = std::chrono::seconds(30);
    const auto staleResult = StarterArenaProject().parent_path() /
        "Build/Editor/Play/validated-graceful-stop/runtime-result-v1.json";
    std::filesystem::create_directories(staleResult.parent_path());
    std::ofstream(staleResult, std::ios::binary | std::ios::trunc) <<
        "{\"schemaVersion\":1,\"playSessionId\":\"prior-session\"}\n";
    ASSERT_TRUE(controller.Start(workspace, request));
    const auto runningDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while ((controller.Snapshot().state == SagaEditor::RuntimePlayState::Preparing ||
            controller.Snapshot().state == SagaEditor::RuntimePlayState::Starting) &&
           std::chrono::steady_clock::now() < runningDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ASSERT_EQ(controller.Snapshot().state, SagaEditor::RuntimePlayState::Running);

    controller.Stop();
    controller.Stop();
    controller.Shutdown();
    const auto snapshot = controller.Snapshot();
    ASSERT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Completed)
        << snapshot.diagnosticCode << ": " << snapshot.message;
    ASSERT_TRUE(snapshot.runtimeResult.has_value());
    EXPECT_TRUE(snapshot.runtimeResult->Successful());
    EXPECT_EQ(snapshot.runtimeResult->shutdownClassification, "GracefulStop");
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.GracefulStop");
}

#ifndef _WIN32
TEST(RuntimeProcessControllerTests, RuntimeTimeoutIgnoringSigtermFailsWithinSigkillBound)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    auto request = HelperRequest("runtime-timeout-forced", "ready-hang");
    request.runtimeTimeout = std::chrono::milliseconds(150);
    const auto startedAt = std::chrono::steady_clock::now();
    ASSERT_TRUE(controller.Start(workspace, request));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();

    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.RuntimeTimeout");
    EXPECT_NE(snapshot.diagnosticCode, "Play.Runtime.ForcedStop");
    EXPECT_EQ(snapshot.shutdownClassification,
              SagaEditor::ToolProcessShutdownClassification::Forced);
    EXPECT_LT(std::chrono::steady_clock::now() - startedAt, std::chrono::seconds(6));
}

TEST(RuntimeProcessControllerTests, ExplicitStopAfterReadinessCompletesAsForcedStop)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    auto request = HelperRequest("explicit-forced-stop", "ready-hang");
    ASSERT_TRUE(controller.Start(workspace, request));
    ASSERT_TRUE(WaitForState(controller, SagaEditor::RuntimePlayState::Running));
    controller.Stop();
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();

    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Completed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.ForcedStop");
    EXPECT_EQ(snapshot.shutdownClassification,
              SagaEditor::ToolProcessShutdownClassification::Forced);
}

TEST(RuntimeProcessControllerTests, MissingStartupRecordFailsStartupTimeout)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    auto request = HelperRequest("missing-startup-ready", "hang-no-ready");
    request.startupTimeout = std::chrono::milliseconds(150);
    ASSERT_TRUE(controller.Start(workspace, request));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.StartupTimeout");
}

TEST(RuntimeProcessControllerTests, MalformedStartupRecordFailsClosed)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    ASSERT_TRUE(controller.Start(workspace,
        HelperRequest("malformed-startup-ready", "malformed-ready")));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Runtime.StartupReady.JsonInvalid");
}

TEST(RuntimeProcessControllerTests, WrongSessionStartupRecordFailsClosed)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    ASSERT_TRUE(controller.Start(workspace,
        HelperRequest("wrong-session-startup-ready", "wrong-session")));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Runtime.StartupReady.PlaySessionMismatch");
}

TEST(RuntimeProcessControllerTests, WrongManifestStartupRecordFailsClosed)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    ASSERT_TRUE(controller.Start(workspace,
        HelperRequest("wrong-manifest-startup-ready", "wrong-manifest")));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Runtime.StartupReady.ManifestHashMismatch");
}

TEST(RuntimeProcessControllerTests, ProcessExitBeforeReadinessIsStartupFailure)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    ASSERT_TRUE(controller.Start(workspace,
        HelperRequest("exit-before-startup-ready", "exit-before-ready")));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.StartupFailure");
}

TEST(RuntimeProcessControllerTests, ProcessCrashAfterReadinessIsCrashFailure)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    ASSERT_TRUE(controller.Start(workspace,
        HelperRequest("crash-after-startup-ready", "ready-crash")));
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Failed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.Crash");
}

TEST(RuntimeProcessControllerTests, StopWhileStartingIsBoundedForcedStop)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    SagaEditor::RuntimeProcessController controller;
    auto request = HelperRequest("stop-while-starting", "hang-no-ready");
    ASSERT_TRUE(controller.Start(workspace, request));
    ASSERT_TRUE(WaitForState(controller, SagaEditor::RuntimePlayState::Starting));
    controller.Stop();
    const auto snapshot = WaitForTerminal(controller);
    controller.Shutdown();
    EXPECT_EQ(snapshot.state, SagaEditor::RuntimePlayState::Completed);
    EXPECT_EQ(snapshot.diagnosticCode, "Play.Runtime.ForcedStop");
}
#endif
