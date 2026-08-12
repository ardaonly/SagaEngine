// SPDX-License-Identifier: MPL-2.0

#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <nlohmann/json.hpp>

#include <set>

namespace SagaRuntime::Play
{
namespace
{
using Json = nlohmann::ordered_json;

RuntimeDiagnostic Error(std::string code, std::string message)
{ return {std::move(code), std::move(message), {}}; }

bool ExactFields(const Json& value, std::initializer_list<std::string_view> allowed)
{
    if (!value.is_object()) return false;
    const std::set<std::string_view> names(allowed);
    for (auto iterator = value.cbegin(); iterator != value.cend(); ++iterator)
        if (!names.contains(iterator.key())) return false;
    return true;
}

bool NonEmptyString(const Json& value, std::string_view name)
{
    const auto found = value.find(std::string(name));
    return found != value.end() && found->is_string() &&
        !found->get_ref<const std::string&>().empty();
}

std::string Canonical(const Json& value) { return value.dump(2) + "\n"; }

Json Nullable(const std::optional<std::string>& value)
{ return value ? Json(*value) : Json(nullptr); }

bool ReadNullable(const Json& root, std::string_view name,
                  std::optional<std::string>& output)
{
    const auto found = root.find(std::string(name));
    if (found == root.end()) return false;
    if (found->is_null()) { output.reset(); return true; }
    if (!found->is_string() || found->get_ref<const std::string&>().empty()) return false;
    output = found->get<std::string>();
    return true;
}
}

std::string SerializeCanonicalGameplayState(const CanonicalGameplayState& state)
{
    Json collectibles = Json::array();
    for (const auto& collectible : state.collectibles)
        collectibles.push_back({{"collectibleId", collectible.collectibleId},
            {"state", collectible.state}});
    return Canonical({{"activeImplementationIdentity", Nullable(state.activeImplementationIdentity)},
        {"collectibles", std::move(collectibles)}, {"completionPoint", Nullable(state.completionPoint)},
        {"objectiveMatchResult", Nullable(state.objectiveMatchResult)}, {"packageGeneration", {
            {"generationHash", Nullable(state.packageGenerationHash)},
            {"generationId", Nullable(state.packageGenerationId)},
            {"graphHash", Nullable(state.packageGraphHash)}}},
        {"phases", state.phases}, {"respawn", {
            {"activeCollectibles", state.activeCollectibles},
            {"pendingRespawns", state.pendingRespawns}}},
        {"scenarioIdentity", Nullable(state.scenarioIdentity)},
        {"schema", "saga.gameplay-state.v1"},
        {"score", state.score ? Json(*state.score) : Json(nullptr)}});
}

GameplayStateIdentity ParseCanonicalGameplayState(
    std::string_view bytes, std::string_view expectedHash)
{
    GameplayStateIdentity result;
    Json root;
    try { root = Json::parse(bytes); }
    catch (const Json::exception& error)
    { result.diagnostics.push_back(Error("Runtime.GameplayState.JsonInvalid", error.what())); return result; }
    if (!ExactFields(root, {"activeImplementationIdentity", "collectibles", "completionPoint",
            "objectiveMatchResult", "packageGeneration", "phases", "respawn",
            "scenarioIdentity", "schema", "score"}) ||
        root.value("schema", std::string{}) != "saga.gameplay-state.v1" ||
        !root["collectibles"].is_array() || !root["phases"].is_array() ||
        !ExactFields(root["packageGeneration"], {"generationHash", "generationId", "graphHash"}) ||
        !ExactFields(root["respawn"], {"activeCollectibles", "pendingRespawns"}) ||
        !root["respawn"]["activeCollectibles"].is_number_integer() ||
        !root["respawn"]["pendingRespawns"].is_number_integer() ||
        (!root["score"].is_null() && !root["score"].is_number_integer()))
    { result.diagnostics.push_back(Error("Runtime.GameplayState.SchemaInvalid",
        "Gameplay state must satisfy saga.gameplay-state.v1 exactly.")); return result; }
    CanonicalGameplayState state;
    const auto& package = root["packageGeneration"];
    if (!ReadNullable(root, "scenarioIdentity", state.scenarioIdentity) ||
        !ReadNullable(root, "completionPoint", state.completionPoint) ||
        !ReadNullable(root, "objectiveMatchResult", state.objectiveMatchResult) ||
        !ReadNullable(root, "activeImplementationIdentity", state.activeImplementationIdentity) ||
        !ReadNullable(package, "generationId", state.packageGenerationId) ||
        !ReadNullable(package, "generationHash", state.packageGenerationHash) ||
        !ReadNullable(package, "graphHash", state.packageGraphHash))
        result.diagnostics.push_back(Error("Runtime.GameplayState.NullableIdentityInvalid",
            "Gameplay identities must be non-empty strings or explicit nulls."));
    const bool anyPackage = state.packageGenerationId || state.packageGenerationHash ||
        state.packageGraphHash;
    if ((anyPackage && (!state.packageGenerationId || !state.packageGenerationHash ||
            !state.packageGraphHash)) ||
        (state.packageGenerationHash && !IsSha256(*state.packageGenerationHash)) ||
        (state.packageGraphHash && !IsSha256(*state.packageGraphHash)))
        result.diagnostics.push_back(Error("Runtime.GameplayState.PackageIdentityInvalid",
            "Gameplay package hashes must be full SHA-256 values."));
    state.activeCollectibles = root["respawn"]["activeCollectibles"].get<std::int64_t>();
    state.pendingRespawns = root["respawn"]["pendingRespawns"].get<std::int64_t>();
    if (root["score"].is_number_integer()) state.score = root["score"].get<std::int64_t>();
    if (state.activeCollectibles < 0 || state.pendingRespawns < 0 ||
        (state.score && *state.score < 0))
        result.diagnostics.push_back(Error("Runtime.GameplayState.CountInvalid",
            "Gameplay score and respawn counts must be non-negative."));
    for (const auto& phase : root["phases"])
        if (phase.is_string() && !phase.get_ref<const std::string&>().empty())
            state.phases.push_back(phase.get<std::string>());
        else result.diagnostics.push_back(Error("Runtime.GameplayState.PhaseInvalid",
            "Gameplay phases must be ordered strings."));
    std::set<std::string> collectibleIds;
    for (const auto& collectible : root["collectibles"])
        if (ExactFields(collectible, {"collectibleId", "state"}) &&
            NonEmptyString(collectible, "collectibleId") && NonEmptyString(collectible, "state") &&
            collectibleIds.insert(collectible["collectibleId"].get<std::string>()).second &&
            (collectible["state"] == "Active" || collectible["state"] == "Collected" ||
             collectible["state"] == "Respawning" || collectible["state"] == "Respawned"))
            state.collectibles.push_back({collectible["collectibleId"].get<std::string>(),
                collectible["state"].get<std::string>()});
        else result.diagnostics.push_back(Error("Runtime.GameplayState.CollectibleInvalid",
            "Each ordered collectible requires an identity and state."));
    result.canonicalBytes = SerializeCanonicalGameplayState(state);
    result.hash = Sha256Bytes(result.canonicalBytes);
    if (result.canonicalBytes != bytes)
        result.diagnostics.push_back(Error("Runtime.GameplayState.NonCanonical",
            "Gameplay state must use canonical ordered UTF-8/LF JSON bytes."));
    if (!expectedHash.empty() && result.hash != expectedHash)
        result.diagnostics.push_back(Error("Runtime.GameplayState.HashMismatch",
            "Gameplay state bytes do not match the declared SHA-256 identity."));
    if (result.diagnostics.empty()) result.state = std::move(state);
    return result;
}
} // namespace SagaRuntime::Play
