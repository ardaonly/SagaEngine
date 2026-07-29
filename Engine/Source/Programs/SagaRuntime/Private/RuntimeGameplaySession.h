// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "RuntimeScriptSession.h"
#include "RuntimeTriggerSession.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string>

namespace SagaRuntimeApp
{

struct RuntimeGameplayOwnershipCounters
{
    std::size_t runtimeEntities = 0;
    std::size_t scriptInstances = 0;
    std::size_t triggerBindings = 0;
    std::size_t activeTriggerOverlaps = 0;
    std::size_t stateKeys = 0;
    std::size_t pendingCollectibleRespawns = 0;

    bool operator==(const RuntimeGameplayOwnershipCounters&) const = default;
};

/// Single simulation-thread owner shared by graphical and headless adapters.
class RuntimeGameplaySession final
{
public:
    using FixedUpdate = std::function<bool(double)>;
    using LiveOwnershipQuery =
        std::function<RuntimeGameplayOwnershipCounters()>;

    RuntimeGameplaySession(RuntimeScriptInputPort& input,
        RuntimeTriggerSession& triggers, FixedUpdate update,
        LiveOwnershipQuery liveOwnershipQuery)
        : input_(input), triggers_(triggers), update_(std::move(update)),
          liveOwnershipQuery_(std::move(liveOwnershipQuery)) {}

    [[nodiscard]] bool Advance(double deltaSeconds,
        std::set<std::string> pressed = {},
        std::set<std::string> held = {});
    void Reset() noexcept;
    [[nodiscard]] RuntimeGameplayOwnershipCounters
        LiveOwnershipCounters() const;
    [[nodiscard]] std::uint64_t TickCount() const noexcept { return tickCount_; }

private:
    static constexpr double FixedDelta = 1.0 / 60.0;
    static constexpr double MaximumCatchUp = 0.25;
    RuntimeScriptInputPort& input_;
    RuntimeTriggerSession& triggers_;
    FixedUpdate update_;
    LiveOwnershipQuery liveOwnershipQuery_;
    double accumulator_ = 0;
    std::uint64_t tickCount_ = 0;
};

} // namespace SagaRuntimeApp
