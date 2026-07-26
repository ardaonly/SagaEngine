// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/RuntimeProcessController.h"
#include "SagaEditor/Play/PlayPreparationPipeline.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "TestSourceRoot.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
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

TEST(StarterArenaPlayStopIntegrationTests,
     SeparateRuntimeDoesNotRequireProjectionArtifacts)
{
    const auto root = std::filesystem::temp_directory_path() /
        ("saga-projection-independent-play-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::copy(StarterArenaProject().parent_path(), root,
        std::filesystem::copy_options::recursive);
    std::error_code cleanupError;
    std::filesystem::remove_all(root / "Build/Editor", cleanupError);
    std::filesystem::remove_all(root / ".saga", cleanupError);
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(root / "StarterArena.sagaproj"));
    const auto prepared = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace,
        {"projection-independent-play", RuntimeExecutable(), {}, {}, {}});
    ASSERT_TRUE(prepared.passed) << (prepared.diagnostics.empty() ? "" :
        prepared.diagnostics.front().code + ": " + prepared.diagnostics.front().message);

    std::size_t removed = 0;
    const auto artifacts = root / "Build/Editor/Scripting/Artifacts";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(artifacts))
        if (entry.is_regular_file() && entry.path().filename() == "block_projection_v1.json")
        {
            std::filesystem::remove(entry.path());
            ++removed;
        }
    ASSERT_GT(removed, 0U);

    SagaEditor::ToolProcessRequest request;
    request.executable = RuntimeExecutable();
    request.arguments = {"--play-envelope", prepared.localEnvelopePath.string(),
        "--headless", "--smoke-interactions"};
    request.workingDirectory = root;
    request.timeout = std::chrono::seconds(60);
    const auto process = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_TRUE(process.Succeeded()) << process.diagnosticCode << ": "
        << process.diagnosticMessage << "\n" << process.standardError;
    ASSERT_TRUE(std::filesystem::is_regular_file(prepared.resultPath));
    std::ifstream resultInput(prepared.resultPath, std::ios::binary);
    const auto result = nlohmann::json::parse(resultInput);
    EXPECT_EQ(result.value("status", std::string{}), "Succeeded");
    EXPECT_EQ(result.value("interactionActionCount", 0), 2);
    std::filesystem::remove_all(root, cleanupError);
}

TEST(StarterArenaGraphicalRuntimeIntegrationTests,
     NamedFramesBindHudSemanticsAcrossCanonicalInputVariants)
{
    if (!std::getenv("SAGA_REQUIRE_REAL_RUNTIME_RENDER"))
        GTEST_SKIP() << "Set SAGA_REQUIRE_REAL_RUNTIME_RENDER=1 on a graphics-capable desktop.";
    const auto* retainedRoot = std::getenv("SAGA_GRAPHICAL_EVIDENCE_ROOT");
    const bool retainEvidence = retainedRoot && *retainedRoot;
    const auto root = retainEvidence
        ? std::filesystem::path(retainedRoot)
        : std::filesystem::temp_directory_path() /
            ("saga-named-graphical-proof-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    const auto run = [&](const std::string& label) -> nlohmann::json
    {
        const auto projectRoot = root / ("variant-" + label);
        std::filesystem::copy(StarterArenaProject().parent_path(), projectRoot,
            std::filesystem::copy_options::recursive);
        std::filesystem::remove_all(projectRoot / "Build/Editor", cleanupError);
        std::filesystem::remove_all(projectRoot / ".saga", cleanupError);
        if (label == "F")
        {
            const auto inputPath = projectRoot / "Input/Input.saga.json";
            std::ifstream input(inputPath, std::ios::binary);
            auto source = nlohmann::json::parse(input);
            source["actions"][0]["bindings"][0]["control"] = "F";
            source["actions"][0]["bindings"][0]["displayName"] = "Keyboard F";
            std::ofstream output(inputPath, std::ios::binary | std::ios::trunc);
            output << source.dump(2) << '\n';
        }
        if (label == "Style")
        {
            const auto stylePath =
                projectRoot / "Content/UI/HUD/interaction-prompt.rcss";
            std::ifstream input(stylePath, std::ios::binary);
            std::string source{std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>()};
            const auto original = std::string{"rgba(12, 18, 28, 210)"};
            const auto replacement = std::string{"rgba(220, 20, 20, 255)"};
            const auto position = source.find(original);
            if (position == std::string::npos)
            {
                ADD_FAILURE() << "StarterArena stylesheet authority token is missing.";
                return {};
            }
            source.replace(position, original.size(), replacement);
            std::ofstream output(stylePath, std::ios::binary | std::ios::trunc);
            output << source;
        }
        SagaEditor::EditorDocumentWorkspace workspace;
        if (!workspace.OpenProject(projectRoot / "StarterArena.sagaproj"))
        {
            ADD_FAILURE() << "Could not open graphical variant " << label;
            return {};
        }
        const auto session = "named-graphical-" + std::string(
            1, static_cast<char>(std::tolower(static_cast<unsigned char>(label.front()))));
        const auto prepared = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace,
            {session, RuntimeExecutable(), {}, {}, {}});
        if (!prepared.passed)
        {
            ADD_FAILURE() << "Could not prepare graphical variant " << label << " at " <<
                projectRoot << ": " <<
                (prepared.diagnostics.empty() ? std::string{} :
                    prepared.diagnostics.front().code + ": " +
                    prepared.diagnostics.front().message) << "\n" <<
                prepared.scriptAnalysis.packageProcess.diagnosticCode << ": " <<
                prepared.scriptAnalysis.packageProcess.diagnosticMessage << "\n" <<
                prepared.scriptAnalysis.packageProcess.standardOutput << "\n" <<
                prepared.scriptAnalysis.packageProcess.standardError;
            return {};
        }
        SagaEditor::ToolProcessRequest processRequest;
        processRequest.executable = RuntimeExecutable();
        processRequest.arguments = {"--play-envelope", prepared.localEnvelopePath.string(),
                                    "--smoke-interactions"};
        processRequest.workingDirectory = projectRoot;
        processRequest.timeout = std::chrono::seconds(60);
        const auto process = SagaEditor::ToolProcessRunner{}.Run(processRequest);
        if (!process.Succeeded())
        {
            std::string resultBytes;
            std::ifstream resultInput(prepared.resultPath, std::ios::binary);
            if (resultInput)
                resultBytes.assign(std::istreambuf_iterator<char>(resultInput),
                                   std::istreambuf_iterator<char>());
            ADD_FAILURE() << projectRoot << " exit=" << process.exitCode << " " <<
                process.diagnosticCode << ": " << process.diagnosticMessage << "\n" <<
                process.standardOutput << "\n" << process.standardError << "\n" << resultBytes;
            return {};
        }
        std::ifstream resultInput(prepared.resultPath, std::ios::binary);
        return nlohmann::json::parse(resultInput);
    };

    const auto eResult = run("E");
    const auto fResult = run("F");
    const auto styleResult = run("Style");
    ASSERT_FALSE(eResult.empty());
    ASSERT_FALSE(fResult.empty());
    ASSERT_FALSE(styleResult.empty());
    EXPECT_EQ(eResult.value("status", std::string{}), "Succeeded");
    EXPECT_EQ(fResult.value("status", std::string{}), "Succeeded");
    EXPECT_EQ(styleResult.value("status", std::string{}), "Succeeded");
    EXPECT_EQ(eResult["observedState"].value("transition", std::string{}),
              "ClosedOpenClosed");
    EXPECT_EQ(fResult["observedState"].value("transition", std::string{}),
              "ClosedOpenClosed");

    const auto captures = [&](const nlohmann::json& result)
    {
        std::map<std::string, nlohmann::json> byName;
        std::set<std::uint64_t> frames;
        for (const auto& capture : result["observedState"]["evidenceCaptures"])
        {
            const auto name = capture.value("name", std::string{});
            EXPECT_FALSE(name.empty());
            EXPECT_TRUE(byName.emplace(name, capture).second) << name;
            if (name != "world-baseline")
            {
                EXPECT_TRUE(frames.insert(capture.value("frameIndex", 0ULL)).second) << name;
            }
            EXPECT_EQ(capture.value("sessionId", std::string{}),
                      result.value("playSessionId", std::string{}));
            EXPECT_EQ(capture.value("canonicalLaunchHash", std::string{}),
                      result.value("canonicalLaunchManifestHash", std::string{}));
            EXPECT_FALSE(capture.value("sha256", std::string{}).empty());
            EXPECT_TRUE(std::filesystem::is_regular_file(
                capture.value("filePath", std::string{})));
            EXPECT_EQ(capture.value("preOverlayFrameIndex", ~0ULL),
                      capture.value("frameIndex", 0ULL));
            EXPECT_TRUE(std::filesystem::is_regular_file(
                capture.value("preOverlayPath", std::string{})));
            EXPECT_FALSE(capture.value("preOverlaySha256", std::string{}).empty());
            EXPECT_GT(capture.value("pixelRatio", 0.0), 0.0);
            const auto& region = capture["hudRegion"];
            EXPECT_LE(region.value("x", 0U) + region.value("width", 0U),
                      capture.value("width", 0U));
            EXPECT_LE(region.value("y", 0U) + region.value("height", 0U),
                      capture.value("height", 0U));
        }
        return byName;
    };
    const auto eCaptures = captures(eResult);
    const auto fCaptures = captures(fResult);
    const auto styleCaptures = captures(styleResult);
    for (const auto* name : {"world-baseline", "e-open", "e-close",
                             "e-reopened", "e-resized"})
        EXPECT_TRUE(eCaptures.contains(name)) << name;
    for (const auto* name : {"world-baseline", "f-open", "f-close"})
        EXPECT_TRUE(fCaptures.contains(name)) << name;

    const auto assertSemantic = [](const nlohmann::json& capture,
        const char* binding, const char* prompt, const bool state)
    {
        EXPECT_EQ(capture.value("promptBindingLabel", std::string{}), binding);
        EXPECT_EQ(capture.value("promptSemanticLabel", std::string{}), prompt);
        EXPECT_EQ(capture.value("doorState", !state), state);
        EXPECT_GE(capture.value("baselineDifferencePixelCount", 0ULL), 32U);
        EXPECT_GE(capture.value("baselineRgbAbsoluteDifference", 0ULL), 2048U);
    };
    ASSERT_TRUE(eCaptures.contains("e-open") && eCaptures.contains("e-close") &&
                eCaptures.contains("e-resized") && fCaptures.contains("f-open"));
    assertSemantic(eCaptures.at("e-open"), "E", "[E] Open Door", false);
    assertSemantic(eCaptures.at("e-close"), "E", "[E] Close Door", true);
    assertSemantic(eCaptures.at("e-resized"), "E", "[E] Open Door", false);
    assertSemantic(fCaptures.at("f-open"), "F", "[F] Open Door", false);
    EXPECT_GE(eCaptures.at("e-close").value(
        "priorSemanticDifferencePixelCount", 0ULL), 16U);
    EXPECT_GE(eCaptures.at("e-close").value(
        "priorSemanticRgbAbsoluteDifference", 0ULL), 1024U);
    EXPECT_EQ(eCaptures.at("world-baseline").value("filePath", std::string{}),
              eCaptures.at("e-open").value("preOverlayPath", std::string{}));
    EXPECT_EQ(eCaptures.at("world-baseline").value("frameIndex", ~0ULL),
              eCaptures.at("e-open").value("frameIndex", 0ULL));
    EXPECT_NE(eCaptures.at("e-resized").value("width", 0U),
              eCaptures.at("world-baseline").value("width", 0U));

    const auto& eOpen = eCaptures.at("e-open");
    const auto& fOpen = fCaptures.at("f-open");
    ASSERT_TRUE(styleCaptures.contains("e-open"));
    const auto& styleOpen = styleCaptures.at("e-open");
    EXPECT_NE(eOpen.value("canonicalLaunchHash", std::string{}),
              fOpen.value("canonicalLaunchHash", std::string{}));
    EXPECT_NE(eOpen.value("inputArtifactHash", std::string{}),
              fOpen.value("inputArtifactHash", std::string{}));
    for (const auto* field : {"uiArtifactHash", "sceneHash", "scriptAssemblyHash",
                              "packageGenerationHash"})
        EXPECT_EQ(eOpen.value(field, std::string{}), fOpen.value(field, std::string{}))
            << field;
    EXPECT_EQ(fOpen.value("promptSemanticLabel", std::string{}).find("[E]"),
              std::string::npos);
    for (const auto* field : {"inputArtifactHash", "sceneHash",
                              "scriptAssemblyHash", "packageGenerationHash",
                              "uiDocumentHash", "preOverlaySha256"})
        EXPECT_EQ(eOpen.value(field, std::string{}),
                  styleOpen.value(field, std::string{})) << field;
    for (const auto* field : {"canonicalLaunchHash", "uiArtifactHash", "sha256"})
        EXPECT_NE(eOpen.value(field, std::string{}),
                  styleOpen.value(field, std::string{})) << field;
    EXPECT_NE(eOpen["uiStylesheetHashes"], styleOpen["uiStylesheetHashes"]);
    EXPECT_TRUE(eResult["observedState"].value("meaningfulRendererPixels", false));
    EXPECT_TRUE(fResult["observedState"].value("meaningfulRendererPixels", false));
    EXPECT_TRUE(styleResult["observedState"].value("meaningfulRendererPixels", false));
    EXPECT_GT(eResult["observedState"].value("drawCount", 0U), 0U);
    EXPECT_GT(fResult["observedState"].value("drawCount", 0U), 0U);

    if (!retainEvidence)
        std::filesystem::remove_all(root, cleanupError);
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
