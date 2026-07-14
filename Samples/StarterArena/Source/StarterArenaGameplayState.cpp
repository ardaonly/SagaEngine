/// @file StarterArenaGameplayState.cpp
/// @brief StarterArena gameplay state facade implementation.

#include "StarterArenaGameplayState.h"

#include <SagaShared/Diagnostics/DiagnosticSeverity.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace SagaRuntimeApp
{
namespace
{
using namespace SagaEngine::Scripting;

constexpr const char* kReachable = "starter.pickup.0.reachable";
constexpr const char* kCollected = "starter.pickup.0.collected";
constexpr const char* kScore = "starter.score";
constexpr const char* kPlayerState = "starter.player.state";

ScriptDiagnostic Diagnostic(std::string code, std::string message)
{
    ScriptDiagnostic result;
    result.diagnostic.severity = SagaShared::Diagnostics::DiagnosticSeverity::Error;
    result.diagnostic.code.value = std::move(code);
    result.diagnostic.title = "StarterArena gameplay operation rejected";
    result.diagnostic.message = std::move(message);
    return result;
}

template <typename Result>
Result Rejected(std::string code, std::string message)
{
    Result result;
    result.diagnostics.push_back(Diagnostic(std::move(code), std::move(message)));
    return result;
}

std::string Bool(bool value) { return value ? "true" : "false"; }

} // namespace

StarterArenaGameplayFacade::StarterArenaGameplayFacade(StarterArenaGameplayState& state)
    : state_(state)
{
}

void StarterArenaGameplayFacade::SetPhase(std::string phase)
{
    state_.phase = std::move(phase);
}

void StarterArenaGameplayFacade::SetRuntimeSnapshot(
    std::uint32_t tick, StarterArenaVec2 playerPosition)
{
    state_.tick = tick;
    state_.playerPosition = playerPosition;
    if (PickupReachable() && !state_.firstReachableTick)
    {
        state_.firstReachableTick = tick;
        state_.firstReachablePosition = playerPosition;
    }
}

bool StarterArenaGameplayFacade::PickupReachable() const noexcept
{
    if (state_.pickupCollected) return false;
    const double dx = state_.playerPosition.x - state_.pickup.position.x;
    const double dy = state_.playerPosition.y - state_.pickup.position.y;
    return dx * dx + dy * dy <= state_.pickup.radius * state_.pickup.radius;
}

SagaEngine::Scripting::ScriptStateBoolResult
StarterArenaGameplayFacade::GetBool(const std::string& key) const
{
    ScriptStateBoolResult result;
    if (key == kReachable) result.value = PickupReachable();
    else if (key == kCollected) result.value = state_.pickupCollected;
    else return Rejected<ScriptStateBoolResult>(
        "StarterArena.Gameplay.InvalidTarget", "Unsupported boolean state key: " + key);
    result.succeeded = true;
    return result;
}

SagaEngine::Scripting::ScriptStateInt32Result
StarterArenaGameplayFacade::AddInt32(const std::string& key, std::int32_t delta)
{
    if (key != kScore)
        return Rejected<ScriptStateInt32Result>(
            "StarterArena.Gameplay.InvalidTarget", "Unsupported integer state key: " + key);
    if ((delta > 0 && state_.score > std::numeric_limits<std::int32_t>::max() - delta) ||
        (delta < 0 && state_.score < std::numeric_limits<std::int32_t>::min() - delta))
        return Rejected<ScriptStateInt32Result>(
            "StarterArena.Gameplay.UnsupportedOperation", "Score mutation would overflow.");
    const auto before = state_.score;
    state_.score += delta;
    state_.mutations.push_back({state_.tick, state_.phase, "AddInt32", key,
                                std::to_string(before), std::to_string(state_.score),
                                "Passed", {}});
    ScriptStateInt32Result result;
    result.succeeded = true;
    result.value = state_.score;
    return result;
}

SagaEngine::Scripting::ScriptHostOperationResult
StarterArenaGameplayFacade::SetBool(const std::string& key, bool value)
{
    if (key != kCollected)
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.InvalidTarget", "Unsupported writable boolean key: " + key);
    if (!value)
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.UnsupportedOperation", "Collected pickup state cannot be reset.");
    if (state_.pickupCollected)
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.PickupAlreadyCollected", "Pickup was already collected.");
    if (!PickupReachable())
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.PickupNotReachable", "Player has not reached the pickup.");
    state_.pickupCollected = true;
    state_.mutationTick = state_.tick;
    state_.mutationPosition = state_.playerPosition;
    state_.mutations.push_back({state_.tick, state_.phase, "SetBool", key,
                                "false", Bool(value), "Passed", {}});
    ScriptHostOperationResult result;
    result.succeeded = true;
    return result;
}

SagaEngine::Scripting::ScriptHostOperationResult
StarterArenaGameplayFacade::SetString(const std::string& key, const std::string& value)
{
    if (key != kPlayerState)
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.InvalidTarget", "Unsupported string state key: " + key);
    if (value != "normal" && value != "powered")
        return Rejected<ScriptHostOperationResult>(
            "StarterArena.Gameplay.UnsupportedOperation", "Unsupported player state: " + value);
    const auto before = state_.playerState;
    state_.playerState = value;
    state_.mutations.push_back({state_.tick, state_.phase, "SetString", key,
                                before, value, before == value ? "NoChange" : "Passed", {}});
    ScriptHostOperationResult result;
    result.succeeded = true;
    return result;
}

} // namespace SagaRuntimeApp
