// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaGameplayState.h
/// @brief App-local deterministic gameplay state and safe script facade.

#pragma once

#include "StarterArenaSmoke.h"

#include <SagaEngine/Scripting/ISagaScriptHost.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

struct StarterArenaPickup
{
    std::string id = "starter.pickup.0";
    StarterArenaVec2 position{0.48, 0.24};
    double radius = 0.05;
    std::int32_t scoreValue = 10;
};

struct StarterArenaGameplayMutation
{
    std::uint32_t tick = 0;
    std::string lifecycleEvent;
    std::string operation;
    std::string target;
    std::string beforeValue;
    std::string afterValue;
    std::string status;
    std::string diagnostic;
};

struct StarterArenaGameplayState
{
    bool enabled = false;
    bool attempted = false;
    bool passed = false;
    std::int32_t score = 0;
    bool pickupCollected = false;
    std::string playerState = "normal";
    StarterArenaPickup pickup;
    StarterArenaVec2 playerPosition;
    std::uint32_t tick = 0;
    std::uint32_t updateCount = 0;
    std::optional<std::uint32_t> firstReachableTick;
    std::optional<std::uint32_t> mutationTick;
    std::optional<StarterArenaVec2> firstReachablePosition;
    std::optional<StarterArenaVec2> mutationPosition;
    std::string lifecycleEvent = "NotStarted";
    std::vector<std::string> callbacksObserved;
    std::vector<StarterArenaGameplayMutation> mutations;
};

class StarterArenaGameplayFacade final : public SagaEngine::Scripting::ISagaScriptStatePort
{
public:
    explicit StarterArenaGameplayFacade(StarterArenaGameplayState& state);

    void SetLifecycleEvent(std::string lifecycleEvent);
    void SetRuntimeSnapshot(std::uint32_t tick, StarterArenaVec2 playerPosition);
    [[nodiscard]] bool PickupReachable() const noexcept;

    [[nodiscard]] SagaEngine::Scripting::ScriptStateBoolResult GetBool(
        const std::string& key) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateInt32Result AddInt32(
        const std::string& key,
        std::int32_t delta) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetBool(
        const std::string& key,
        bool value) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetString(
        const std::string& key,
        const std::string& value) override;

private:
    StarterArenaGameplayState& state_;
};

} // namespace SagaRuntimeApp
