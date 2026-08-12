// SPDX-License-Identifier: MPL-2.0

#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <algorithm>
#include <fstream>

namespace
{
using Json = nlohmann::json;
using namespace SagaRuntime::Play;

std::filesystem::path Temporary(std::string_view name)
{
    return std::filesystem::temp_directory_path() /
        ("saga-runtime-play-" + std::string(name) + "-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
}

void Write(const std::filesystem::path& path, const std::string& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string ArtifactBytes(std::string_view id, std::string_view type, Json payload)
{
    const auto payloadBytes = payload.dump(2) + "\n";
    Json artifact = {
        {"artifactId", id}, {"artifactType", type},
        {"contentHash", Sha256Bytes(payloadBytes)},
        {"cooker", {{"name", "sagapack"}, {"version", "0.0.11"}}},
        {"dependencies", Json::array()}, {"payload", std::move(payload)},
        {"provenance", nullptr}, {"schemaVersion", 1},
        {"sourceHash", "sha256:" + std::string(64, '1')},
    };
    return artifact.dump(2) + "\n";
}

Json ScenePayload()
{
    return {
        {"entities", Json::array({
            {{"components", Json::array({
                {{"position", {0, 0, 0}}, {"rotation", {0, 0, 0, 1}},
                 {"scale", {1, 1, 1}}, {"type", "Transform"}},
                {{"behaviorTypeId", "Game.Door"}, {"instanceConfiguration", Json::object()},
                 {"requiredCapabilities", {"World.Transform.ReadWrite"}},
                 {"scriptArtifactLogicalId", "scripts.assembly"}, {"type", "ScriptBehavior"}},
                {{"behaviorTypeId", "Game.Door"}, {"inputActionId", "Interact"},
                 {"namedActionId", "Interact"}, {"type", "ScriptActionBinding"}},
            })}, {"entityId", "door"}, {"name", "Door"}, {"parentEntityId", nullptr}},
        })},
        {"sceneId", "game.scene"},
    };
}

CanonicalRuntimeLaunchManifest Launch()
{
    const auto hash = "sha256:" + std::string(64, 'a');
    CanonicalRuntimeLaunchManifest manifest;
    manifest.projectId = "project";
    manifest.projectHash = hash;
    manifest.playSessionId = "session";
    manifest.scene = {"scene", hash};
    manifest.input = {"input", hash};
    manifest.scriptAssembly = {"scripts.assembly", hash};
    manifest.scriptRuntimeConfig = {"scripts.runtime-config", hash};
    manifest.scriptAssemblyManifest = {"scripts.assembly-manifest", hash};
    manifest.scriptReferenceManifest = {"scripts.reference-manifest", hash};
    manifest.behaviors = {"scripts.behaviors", hash};
    manifest.namedActions = {"scripts.named-actions", hash};
    manifest.capabilities = {"scripts.capabilities", hash};
    manifest.packageGeneration = {"packages", hash};
    manifest.packageGraphHash = hash;
    manifest.scriptSourceRevision = hash;
    manifest.scriptSourceSetHash = hash;
    manifest.runtimeBridge = {"runtime.bridge", hash,
                              "NativeCallbackTable.v3+NamedAction.v1",
                              "net10.0", "0.0.11", "TrustedEngineDistribution"};
    manifest.requiredRuntimeCapabilities = {"World.Transform.ReadWrite"};
    manifest.toolVersions = {{"sagapack", "0.0.11"}, {"sagascript", "0.0.11"}};
    return manifest;
}

std::string RuntimeResultBytes(std::string session = "session",
                               std::string manifestHash = "sha256:" + std::string(64, 'a'),
                               std::string status = "Succeeded", int exitCode = 0)
{
    CanonicalGameplayState gameplay;
    gameplay.scenarioIdentity = "project";
    gameplay.completionPoint = "Completed";
    gameplay.collectibles = {{"core-a", "Collected"}, {"core-b", "Active"}};
    gameplay.phases = {"PreStart", "Running", "Won"};
    gameplay.score = 10;
    gameplay.activeCollectibles = 1;
    gameplay.objectiveMatchResult = "ChallengeAccepted";
    gameplay.packageGenerationId = "generation";
    gameplay.packageGenerationHash = "sha256:" + std::string(64, 'b');
    gameplay.packageGraphHash = "sha256:" + std::string(64, 'c');
    gameplay.activeImplementationIdentity = "collectibles.lowlevel";
    const auto gameplayBytes = SerializeCanonicalGameplayState(gameplay);
    return Json({{"canonicalGameplayState", gameplayBytes},
        {"canonicalGameplayStateHash", Sha256Bytes(gameplayBytes)},
        {"canonicalLaunchManifestHash", std::move(manifestHash)},
        {"crashArtifactPath", nullptr}, {"exitClassification", "Completed"},
        {"exitCode", exitCode}, {"framesExecuted", 350},
        {"interactionActionCount", 2}, {"logPath", "runtime.log"},
        {"observedState", {{"transition", "ClosedOpenClosed"}}},
        {"playSessionId", std::move(session)}, {"runtimeDiagnostics", Json::array()},
        {"schemaVersion", 1}, {"shutdownClassification", "Clean"},
        {"startupDiagnostics", Json::array()}, {"status", std::move(status)}}).dump(2) + "\n";
}

TEST(RuntimeLaunchCompatibilityTests, WrongRuntimeBridgeAbiIsRejectedByRuntimeContract)
{
    auto launch = Launch();
    launch.runtimeBridge.abi = "NativeCallbackTable.v1";
    const auto parsed = ParseCanonicalManifest(SerializeCanonical(launch));
    ASSERT_FALSE(parsed.Succeeded());
    EXPECT_TRUE(std::any_of(parsed.diagnostics.begin(), parsed.diagnostics.end(),
        [](const auto& diagnostic)
        { return diagnostic.code == "Runtime.Launch.RuntimeBridgeInvalid"; }));
}

class Target final : public INamedActionTarget
{
public:
    bool InvokeNamedAction(std::string_view behavior, std::string_view action,
                           std::string_view entity) override
    {
        invoked = std::string(behavior) + ":" + std::string(action) + ":" + std::string(entity);
        invocations.push_back(invoked);
        return accept;
    }
    bool accept = true;
    std::string invoked;
    std::vector<std::string> invocations;
};
}

TEST(RuntimeLaunchManifestDeterminismTests, CanonicalBytesRoundTripAndRejectAbsoluteFields)
{
    const auto bytes = SerializeCanonical(Launch());
    const auto parsed = ParseCanonicalManifest(bytes, Sha256Bytes(bytes));
    ASSERT_TRUE(parsed.Succeeded());
    EXPECT_EQ(parsed.canonicalBytes, bytes);
    EXPECT_EQ(parsed.hash, Sha256Bytes(bytes));

    auto json = Json::parse(bytes);
    json["workingDirectory"] = "/tmp/escaped";
    const auto rejected = ParseCanonicalManifest(json.dump(2) + "\n");
    EXPECT_FALSE(rejected.Succeeded());
    EXPECT_EQ(rejected.diagnostics.front().code, "Runtime.Launch.ManifestSchemaInvalid");
}

TEST(RuntimeResultV1Tests, AcceptsSuccessfulSessionBoundResult)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    const auto parsed = ParseRuntimeResultV1(RuntimeResultBytes("session", hash),
                                              "session", hash);
    ASSERT_TRUE(parsed.Valid());
    EXPECT_TRUE(parsed.result->Successful());
    EXPECT_EQ(parsed.result->framesExecuted, 350U);
    EXPECT_EQ(parsed.result->interactionActionCount, 2U);
}

TEST(CanonicalGameplayStateTests, RoundTripsExactSchemaAndRejectsHostMetadata)
{
    CanonicalGameplayState state;
    state.scenarioIdentity = "arena-rush";
    state.completionPoint = "DeterministicEndpoint";
    state.collectibles = {{"core-1", "Collected"}, {"core-2", "Respawned"}};
    state.phases = {"PreStart", "Running", "Won"};
    state.score = 100;
    state.activeCollectibles = 2;
    state.objectiveMatchResult = "Won";
    state.packageGenerationId = "generation";
    state.packageGenerationHash = "sha256:" + std::string(64, 'd');
    state.packageGraphHash = "sha256:" + std::string(64, 'e');
    state.activeImplementationIdentity = "collectibles.lowlevel";
    const auto bytes = SerializeCanonicalGameplayState(state);
    const auto parsed = ParseCanonicalGameplayState(bytes, Sha256Bytes(bytes));
    ASSERT_TRUE(parsed.Valid());
    EXPECT_EQ(parsed.canonicalBytes, bytes);
    EXPECT_EQ(parsed.state->collectibles[1].collectibleId, "core-2");

    auto extra = Json::parse(bytes);
    extra["hostPath"] = "/tmp/not-semantic";
    EXPECT_FALSE(ParseCanonicalGameplayState(extra.dump(2) + "\n").Valid());
}

TEST(CanonicalGameplayStateTests, SerializesUnavailableIdentityAndScoreAsExplicitNull)
{
    const auto bytes = SerializeCanonicalGameplayState({});
    const auto json = Json::parse(bytes);
    EXPECT_TRUE(json["scenarioIdentity"].is_null());
    EXPECT_TRUE(json["score"].is_null());
    EXPECT_TRUE(json["packageGeneration"]["generationId"].is_null());
    const auto parsed = ParseCanonicalGameplayState(bytes, Sha256Bytes(bytes));
    ASSERT_TRUE(parsed.Valid());
    EXPECT_FALSE(parsed.state->scenarioIdentity.has_value());
}

TEST(RuntimeStartupReadyV1Tests, AcceptsCanonicalSessionBoundRecord)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    const RuntimeStartupReadyV1 ready{1, "session", hash, "Ready"};
    const auto bytes = SerializeRuntimeStartupReadyV1(ready);
    const auto parsed = ParseRuntimeStartupReadyV1(bytes, "session", hash);
    ASSERT_TRUE(parsed.Valid());
    EXPECT_EQ(parsed.ready->playSessionId, "session");
    EXPECT_EQ(parsed.ready->canonicalLaunchManifestHash, hash);
}

TEST(RuntimeStartupReadyV1Tests, RejectsMissingMalformedAndUnsupportedRecords)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    EXPECT_FALSE(ParseRuntimeStartupReadyV1({}, "session", hash).Valid());
    EXPECT_FALSE(ParseRuntimeStartupReadyV1("not-json", "session", hash).Valid());
    auto unsupported = Json::parse(SerializeRuntimeStartupReadyV1(
        {1, "session", hash, "Ready"}));
    unsupported["schemaVersion"] = 2;
    EXPECT_FALSE(ParseRuntimeStartupReadyV1(
        unsupported.dump(2) + "\n", "session", hash).Valid());
}

TEST(RuntimeStartupReadyV1Tests, RejectsWrongSessionAndCanonicalManifestHash)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    const auto otherHash = "sha256:" + std::string(64, 'b');
    EXPECT_FALSE(ParseRuntimeStartupReadyV1(SerializeRuntimeStartupReadyV1(
        {1, "old-session", hash, "Ready"}), "session", hash).Valid());
    EXPECT_FALSE(ParseRuntimeStartupReadyV1(SerializeRuntimeStartupReadyV1(
        {1, "session", otherHash, "Ready"}), "session", hash).Valid());
}

TEST(RuntimeResultV1Tests, RejectsWrongSessionManifestMalformedAndUnsupportedSchema)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    EXPECT_FALSE(ParseRuntimeResultV1(RuntimeResultBytes("old", hash), "current", hash).Valid());
    EXPECT_FALSE(ParseRuntimeResultV1(RuntimeResultBytes("session",
        "sha256:" + std::string(64, 'b')), "session", hash).Valid());
    EXPECT_FALSE(ParseRuntimeResultV1("not-json", "session", hash).Valid());
    auto unsupported = Json::parse(RuntimeResultBytes("session", hash));
    unsupported["schemaVersion"] = 2;
    EXPECT_FALSE(ParseRuntimeResultV1(unsupported.dump(2) + "\n", "session", hash).Valid());
    auto unknownClassification = Json::parse(RuntimeResultBytes("session", hash));
    unknownClassification["exitClassification"] = "Anything";
    EXPECT_FALSE(ParseRuntimeResultV1(
        unknownClassification.dump(2) + "\n", "session", hash).Valid());
}

TEST(RuntimeResultV1Tests, KeepsStructuredFailureValidButNotSuccessful)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    const auto parsed = ParseRuntimeResultV1(
        RuntimeResultBytes("session", hash, "Failed", 17), "session", hash);
    ASSERT_TRUE(parsed.Valid());
    EXPECT_FALSE(parsed.result->Successful());
    EXPECT_EQ(parsed.result->exitCode, 17);
}

TEST(RuntimeResultV1Tests, RejectsNonCanonicalGameplayBytesEvenWithMatchingHash)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    auto result = Json::parse(RuntimeResultBytes("session", hash));
    auto gameplay = result["canonicalGameplayState"].get<std::string>();
    gameplay.insert(gameplay.find('{') + 1, " ");
    result["canonicalGameplayState"] = gameplay;
    result["canonicalGameplayStateHash"] = Sha256Bytes(gameplay);
    const auto parsed = ParseRuntimeResultV1(result.dump(2) + "\n", "session", hash);
    ASSERT_FALSE(parsed.Valid());
    EXPECT_EQ(parsed.diagnostics.front().code, "Runtime.Result.GameplayStateInvalid");
}

TEST(RuntimeResultV1Tests, SuccessfulResultRequiresCompleteGameplayIdentity)
{
    const auto hash = "sha256:" + std::string(64, 'a');
    auto result = Json::parse(RuntimeResultBytes("session", hash));
    const auto absent = SerializeCanonicalGameplayState({});
    result["canonicalGameplayState"] = absent;
    result["canonicalGameplayStateHash"] = Sha256Bytes(absent);
    const auto parsed = ParseRuntimeResultV1(result.dump(2) + "\n", "session", hash);
    ASSERT_FALSE(parsed.Valid());
    EXPECT_EQ(parsed.diagnostics.front().code, "Runtime.Result.GameplayIdentityMissing");
}

TEST(RuntimeLaunchEnvelopeResolutionTests, RejectsTraversalAndUnallowlistedEnvironment)
{
    const auto root = Temporary("envelope");
    const auto manifestPath = root / "launch.json";
    const auto bytes = SerializeCanonical(Launch());
    Write(manifestPath, bytes);
    ResolvedLocalLaunchEnvelope envelope;
    envelope.playSessionId = "session";
    envelope.canonicalManifestPath = manifestPath;
    envelope.canonicalManifestHash = Sha256Bytes(bytes);
    envelope.runtimeExecutable = root / "bin/SagaRuntime";
    envelope.workingDirectory = root / "work";
    envelope.resultPath = root / "result.json";
    envelope.logPath = root / "runtime.log";
    envelope.stopPath = root / "stop.json";
    envelope.startupReadyPath = root / "startup-ready.json";
    envelope.artifacts["scene"] = root / "objects/scene.json";
    EXPECT_TRUE(ValidateLocalEnvelope(envelope, {root}).empty());
    envelope.artifacts["scene"] = root / "../escaped.json";
    envelope.environment["LD_PRELOAD"] = "untrusted";
    const auto diagnostics = ValidateLocalEnvelope(envelope, {root});
    EXPECT_EQ(diagnostics.size(), 2U);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST(RuntimeSceneArtifactLoaderTests, ValidatesHashAndBuildsGenericActionBinding)
{
    const auto root = Temporary("scene");
    const auto scenePath = root / "scene.json";
    Write(scenePath, ArtifactBytes("game.scene", "runtime.scene", ScenePayload()));
    const auto loaded = LoadRuntimeSceneArtifact(scenePath);
    ASSERT_TRUE(loaded.Succeeded());
    ASSERT_EQ(loaded.value->entities.size(), 1U);
    EXPECT_EQ(loaded.value->entities[0].scriptBehavior->scriptArtifactLogicalId,
              "scripts.assembly");

    Target target;
    ScriptActionBindingDispatcher dispatcher(target);
    EXPECT_TRUE(dispatcher.Build(*loaded.value).empty());
    EXPECT_TRUE(dispatcher.Dispatch("Interact").empty());
    EXPECT_EQ(target.invoked, "Game.Door:Interact:door");
    EXPECT_EQ(dispatcher.DispatchCount(), 1U);

    auto tampered = Json::parse(ArtifactBytes("game.scene", "runtime.scene", ScenePayload()));
    tampered["payload"]["entities"][0]["name"] = "Changed";
    Write(scenePath, tampered.dump(2) + "\n");
    const auto rejected = LoadRuntimeSceneArtifact(scenePath);
    ASSERT_FALSE(rejected.Succeeded());
    EXPECT_EQ(rejected.diagnostics.front().code, "Runtime.Artifact.ContentHashMismatch");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST(ScriptActionBindingDispatcherTests, SameBehaviorTypeOnMultipleEntitiesKeepsExactIdentity)
{
    auto payload = ScenePayload();
    auto second = payload["entities"].front();
    second["entityId"] = "door-two";
    second["name"] = "Door Two";
    payload["entities"].push_back(second);
    const auto root = Temporary("multi-instance-binding");
    const auto path = root / "scene.json";
    Write(path, ArtifactBytes("game.scene", "runtime.scene", payload));
    const auto loaded = LoadRuntimeSceneArtifact(path);
    ASSERT_TRUE(loaded.Succeeded());
    Target target;
    ScriptActionBindingDispatcher dispatcher(target);
    ASSERT_TRUE(dispatcher.Build(*loaded.value).empty());
    EXPECT_TRUE(dispatcher.Dispatch("Interact").empty());
    EXPECT_EQ(target.invocations, (std::vector<std::string>{
        "Game.Door:Interact:door", "Game.Door:Interact:door-two"}));
    EXPECT_EQ(dispatcher.DispatchCount(), 2U);
    std::filesystem::remove_all(root);
}

TEST(ScriptActionBindingDispatcherTests, MultipleBehaviorTypesResolveExactEntityAndAction)
{
    auto payload = ScenePayload();
    auto second = payload["entities"].front();
    second["entityId"] = "switch";
    second["name"] = "Switch";
    second["components"][1]["behaviorTypeId"] = "Game.Switch";
    second["components"][2]["behaviorTypeId"] = "Game.Switch";
    second["components"][2]["namedActionId"] = "Toggle";
    payload["entities"].push_back(second);
    const auto root = Temporary("multi-behavior-binding");
    const auto path = root / "scene.json";
    Write(path, ArtifactBytes("game.scene", "runtime.scene", payload));
    const auto loaded = LoadRuntimeSceneArtifact(path);
    ASSERT_TRUE(loaded.Succeeded());
    Target target;
    ScriptActionBindingDispatcher dispatcher(target);
    ASSERT_TRUE(dispatcher.Build(*loaded.value).empty());

    EXPECT_TRUE(dispatcher.Dispatch("Interact").empty());
    EXPECT_EQ(target.invocations, (std::vector<std::string>{
        "Game.Door:Interact:door", "Game.Switch:Toggle:switch"}));
    std::filesystem::remove_all(root);
}

TEST(RuntimeInputMapArtifactLoaderTests, RejectsDuplicatePhysicalBindings)
{
    Json payload = {{"actions", Json::array({
        {{"actionId", "Interact"}, {"bindings", Json::array({
            {{"control", "E"}, {"device", "Keyboard"}, {"displayLabel", "E"},
             {"displayName", "Keyboard E"}}})},
         {"displayName", "Interact"}, {"runtimeRole", "Gameplay"}},
        {{"actionId", "Use"}, {"bindings", Json::array({
            {{"control", "E"}, {"device", "Keyboard"}, {"displayLabel", "E"},
             {"displayName", "Keyboard E"}}})},
         {"displayName", "Use"}, {"runtimeRole", "Gameplay"}},
    })}, {"inputMapId", "game.input"}};
    const auto root = Temporary("input");
    const auto path = root / "input.json";
    Write(path, ArtifactBytes("game.input", "runtime.input-map", payload));
    const auto result = LoadRuntimeInputMapArtifact(path);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.code == "Runtime.Input.BindingAmbiguous"; }));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST(RuntimeSceneArtifactLoaderTests, RejectsDuplicateIdsMissingParentsAndCycles)
{
    const auto root = Temporary("scene-structure");
    const auto path = root / "scene.json";
    auto duplicate = ScenePayload();
    duplicate["entities"].push_back(duplicate["entities"].front());
    Write(path, ArtifactBytes("game.scene", "runtime.scene", duplicate));
    auto result = LoadRuntimeSceneArtifact(path);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.code == "Runtime.Scene.EntityDuplicate"; }));

    auto missing = ScenePayload();
    missing["entities"][0]["parentEntityId"] = "missing";
    Write(path, ArtifactBytes("game.scene", "runtime.scene", missing));
    result = LoadRuntimeSceneArtifact(path);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.code == "Runtime.Scene.ParentMissing"; }));

    auto cycle = ScenePayload();
    auto second = cycle["entities"].front();
    second["entityId"] = "parent";
    second["name"] = "Parent";
    second["parentEntityId"] = "door";
    cycle["entities"][0]["parentEntityId"] = "parent";
    cycle["entities"].push_back(second);
    Write(path, ArtifactBytes("game.scene", "runtime.scene", cycle));
    result = LoadRuntimeSceneArtifact(path);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.code == "Runtime.Scene.ParentCycle"; }));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
