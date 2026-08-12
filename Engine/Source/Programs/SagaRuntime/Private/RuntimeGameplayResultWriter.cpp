// SPDX-License-Identifier: MPL-2.0

#include "RuntimeGameplayResultWriter.h"

#include "RuntimeLaunchSession.h"

namespace SagaRuntimeApp
{
bool WriteRuntimeGameplayResult(
    const RuntimeGameplayResultContext& context,
    const std::vector<SagaRuntime::Play::RuntimeDiagnostic>& runtimeDiagnostics,
    const std::string& status, const std::string& classification,
    std::uint64_t frames, std::uint64_t actions,
    const nlohmann::json& observed)
{
    using Json = nlohmann::json;
    namespace Play = SagaRuntime::Play;
    Json diagnostics = Json::array();
    for (const auto& diagnostic : runtimeDiagnostics)
        diagnostics.push_back({{"code", diagnostic.code}, {"message", diagnostic.message},
            {"subject", diagnostic.subject}});
    const auto phases = observed.value("challengePhases", Json::array());
    const auto lifecycle = observed.value("collectibleLifecycle", Json::object());
    const auto topology = observed.value("staticTopology", Json::object());
    Play::CanonicalGameplayState state;
    if (!context.scenarioIdentity.empty()) state.scenarioIdentity = context.scenarioIdentity;
    state.completionPoint = classification;
    state.objectiveMatchResult = observed.value("transition", "Incomplete");
    if (!context.packageGenerationId.empty())
        state.packageGenerationId = context.packageGenerationId;
    if (!context.packageGenerationHash.empty())
        state.packageGenerationHash = context.packageGenerationHash;
    if (!context.packageGraphHash.empty()) state.packageGraphHash = context.packageGraphHash;
    if (!context.collectibleImplementation.empty())
        state.activeImplementationIdentity = context.collectibleImplementation;
    if (phases.is_array())
        for (const auto& phase : phases)
            if (phase.is_string()) state.phases.push_back(phase.get<std::string>());
    if (topology.contains("collectibleIds") && topology["collectibleIds"].is_array())
        for (const auto& id : topology["collectibleIds"])
            if (id.is_string()) state.collectibles.push_back(
                {id.get<std::string>(), "Active"});
    const auto score = lifecycle.value("score", lifecycle.value("scoreAfterFirst", -1));
    if (score >= 0) state.score = score;
    state.activeCollectibles = lifecycle.value("activeAfterRespawn",
        static_cast<int>(topology.value("collectibles", 0U)));
    state.pendingRespawns = lifecycle.value("pendingAfterRespawn", 0);
    const auto gameplayBytes = Play::SerializeCanonicalGameplayState(state);
    return RuntimeLaunchSession::WriteJson(context.resultPath,
        {{"canonicalLaunchManifestHash", context.canonicalManifestHash},
         {"canonicalGameplayState", gameplayBytes},
         {"canonicalGameplayStateHash", Play::Sha256Bytes(gameplayBytes)},
         {"crashArtifactPath", nullptr}, {"exitClassification", classification},
         {"exitCode", status == "Succeeded" ? 0 : 1}, {"framesExecuted", frames},
         {"interactionActionCount", actions}, {"logPath", context.logPath.string()},
         {"observedState", observed}, {"playSessionId", context.playSessionId},
         {"runtimeDiagnostics", diagnostics}, {"schemaVersion", 1},
         {"startupDiagnostics", Json::array()}, {"status", status},
         {"shutdownClassification", classification == "GracefulStop" ?
             "GracefulStop" : (status == "Succeeded" ? "Clean" : "StartupFailure")}});
}
} // namespace SagaRuntimeApp
