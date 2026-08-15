// SPDX-License-Identifier: MPL-2.0
/// @file SagaLauncherThreeWayParityTests.cpp
/// @brief Editor, published standalone, and direct headless gameplay parity.

#include "SagaLauncherUiTestSupport.h"

#include "SagaEditor/Performance/BuildProfiles.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Standalone/StandaloneBuildCoordinator.h"

namespace
{
bool Terminal(const SagaEditor::RuntimePlayState state)
{
    return state == SagaEditor::RuntimePlayState::Completed ||
        state == SagaEditor::RuntimePlayState::Failed;
}

nlohmann::json RunPublishedStandalone(
    const SagaEditor::StandaloneBuildResult& build)
{
    using Json = nlohmann::json;
    const auto runPath = build.outputDirectory / "StandaloneRun.saga.json";
    std::ifstream runInput(runPath, std::ios::binary);
    const auto run = Json::parse(runInput);
    if (!run["arguments"].is_array() || run["arguments"].size() < 2U)
    {
        ADD_FAILURE() << "Standalone run envelope arguments are invalid.";
        return {};
    }
    const auto envelopePath = std::filesystem::path(
        run["arguments"][1].get<std::string>());
    std::ifstream envelopeInput(envelopePath, std::ios::binary);
    const auto envelope = Json::parse(envelopeInput);

    SagaEditor::ToolProcessRequest request;
    request.executable = run["executable"].get<std::string>();
    for (const auto& argument : run["arguments"])
        request.arguments.push_back(argument.get<std::string>());
    request.arguments.push_back("--headless");
    request.arguments.push_back("--smoke-interactions");
    request.workingDirectory = build.outputDirectory / "Project";
    request.inheritEnvironment = false;
    if (envelope.contains("environment") && envelope["environment"].is_object())
        for (const auto& [name, value] : envelope["environment"].items())
            if (value.is_string()) request.environment[name] = value.get<std::string>();
    for (const char* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                             "XDG_RUNTIME_DIR", "LD_LIBRARY_PATH"})
        if (const char* value = std::getenv(name); value && *value)
            request.environment[name] = value;
    const auto managedRoot = std::filesystem::path(SAGA_BUILD_ROOT) /
        "ManagedToolRuntime/runtime/dotnet";
    request.environment["DOTNET_ROOT"] = managedRoot.string();
    request.environment["DOTNET_ROOT_X64"] = managedRoot.string();
    request.environment["DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"] = "1";
    request.timeout = std::chrono::seconds(120);
    const auto process = SagaEditor::ToolProcessRunner{}.Run(request);
    EXPECT_TRUE(process.Succeeded()) << process.diagnosticCode << ": "
        << process.diagnosticMessage << "\n" << process.standardError;
    if (!process.Succeeded()) return {};
    std::ifstream resultInput(envelope["resultPath"].get<std::string>(),
        std::ios::binary);
    return Json::parse(resultInput);
}

TEST(SagaThreeWayParityTest, CanonicalGameplayBytesAndHashMatchEveryProductPath)
{
    if (QApplication::platformName() != QStringLiteral("xcb"))
        GTEST_SKIP() << "Three-way product parity requires the real Qt xcb path.";
    QTemporaryDir materialization;
    ASSERT_TRUE(materialization.isValid());
    const auto projectRoot = std::filesystem::path(
        materialization.path().toStdString()) / "ArenaRush";
    ASSERT_TRUE(MaterializeArenaRushProject(projectRoot));
    const auto projectPath = projectRoot / "ArenaRush.sagaproj";

    const auto headless = RunAuthoredArenaRush(
        projectRoot, "three-way-headless", 100);
    ASSERT_FALSE(headless.empty());
    ASSERT_EQ(headless.value("status", std::string{}), "Succeeded")
        << headless.dump(2);

    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(projectPath));
    SagaEditor::RuntimeProcessController editorPlay;
    SagaEditor::RuntimeProcessLaunchRequest editorRequest;
    editorRequest.playSessionId = "three-way-editor";
    editorRequest.runtimeExecutable = RuntimeExecutable();
    editorRequest.headless = true;
    editorRequest.smokeInteractions = true;
    editorRequest.startupTimeout = std::chrono::seconds(30);
    editorRequest.runtimeTimeout = std::chrono::seconds(120);
    ASSERT_TRUE(editorPlay.Start(workspace, editorRequest));
    ASSERT_TRUE(WaitUntil([&] { return Terminal(editorPlay.Snapshot().state); }, 150000));
    editorPlay.Shutdown();
    const auto editor = editorPlay.Snapshot();
    ASSERT_EQ(editor.state, SagaEditor::RuntimePlayState::Completed)
        << editor.diagnosticCode << ": " << editor.message;
    ASSERT_TRUE(editor.runtimeResult.has_value());

    const auto profiles = SagaEditor::BuildProfiles::Load(
        projectRoot / "BuildProfiles.saga.json", projectRoot);
    ASSERT_TRUE(profiles.succeeded);
    const auto [profile, profileDiagnostics] =
        SagaEditor::BuildProfiles::CreateRunInput(
            profiles.document, "arena-rush-collectibles");
    ASSERT_TRUE(profileDiagnostics.empty());
    const auto source = SagaEditor::CapturePlaySourceSnapshot(
        workspace, "three-way-standalone");
    ASSERT_TRUE(source.Succeeded());
    std::ifstream runtimeManifestInput(
        TestSourceRoot() / "Config/managed-tool-runtime.json", std::ios::binary);
    const auto runtimeManifest = nlohmann::json::parse(runtimeManifestInput);
    SagaEditor::StandaloneBuildRequest buildRequest;
    buildRequest.source = *source.snapshot;
    buildRequest.buildProfilesPath = projectRoot / "BuildProfiles.saga.json";
    buildRequest.profileId = profile.profileId;
    buildRequest.profileHash = profile.profileHash;
    buildRequest.runtimeIdentifier = runtimeManifest.value("rid", std::string{});
    buildRequest.runtimeExecutable = RuntimeExecutable();
    buildRequest.outputDirectory = std::filesystem::path(
        materialization.path().toStdString()) / "Published";
    buildRequest.toolchain = SagaEditor::DiscoverScriptAnalysisToolchain();
    const auto build = SagaEditor::StandaloneBuildCoordinator{}.Build(buildRequest);
    ASSERT_TRUE(build.Succeeded()) << SagaEditor::StandaloneBuildCoordinator::SerializeReport(build);
    const auto standalone = RunPublishedStandalone(build);
    ASSERT_FALSE(standalone.empty());
    ASSERT_EQ(standalone.value("status", std::string{}), "Succeeded")
        << standalone.dump(2);

    const auto payload = headless["canonicalGameplayState"].get<std::string>();
    const auto hash = headless["canonicalGameplayStateHash"].get<std::string>();
    const auto standalonePayload =
        standalone["canonicalGameplayState"].get<std::string>();
    const auto standaloneHash =
        standalone["canonicalGameplayStateHash"].get<std::string>();
    const bool payloadEquality =
        editor.runtimeResult->canonicalGameplayStateJson == payload &&
        standalonePayload == payload;
    const bool hashEquality =
        editor.runtimeResult->canonicalGameplayStateHash == hash &&
        standaloneHash == hash;
    EXPECT_EQ(editor.runtimeResult->canonicalGameplayStateJson, payload);
    EXPECT_EQ(editor.runtimeResult->canonicalGameplayStateHash, hash);
    EXPECT_EQ(standalonePayload, payload);
    EXPECT_EQ(standaloneHash, hash);
    if (const auto evidenceValue = qgetenv("SAGA_XCB_EVIDENCE_DIR");
        !evidenceValue.isEmpty())
    {
        const auto evidence = std::filesystem::path(evidenceValue.toStdString());
        std::filesystem::create_directories(evidence);
        std::ofstream(evidence / "three-way-canonical-gameplay-state.json",
            std::ios::binary) << payload;
        const nlohmann::json parity = {
            {"schemaVersion", 1},
            {"editor", {{"hash", editor.runtimeResult->canonicalGameplayStateHash},
                         {"payload", editor.runtimeResult->canonicalGameplayStateJson}}},
            {"standalone", {{"hash", standalone["canonicalGameplayStateHash"]},
                             {"payload", standalone["canonicalGameplayState"]}}},
            {"headless", {{"hash", hash}, {"payload", payload}}},
            {"payloadEquality", payloadEquality},
            {"hashEquality", hashEquality}};
        std::ofstream(evidence / "three-way-parity.json", std::ios::binary)
            << parity.dump(2) << '\n';
    }
}
} // namespace
