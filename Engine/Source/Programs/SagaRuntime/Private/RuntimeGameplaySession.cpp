// SPDX-License-Identifier: MPL-2.0
#include "RuntimeGameplaySession.h"

#include <algorithm>
#include <cmath>

namespace SagaRuntimeApp
{

bool RuntimeGameplaySession::Advance(
    const double deltaSeconds, std::set<std::string> pressed,
    std::set<std::string> held)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) return false;
    input_.Advance(std::move(pressed), std::move(held));
    accumulator_ += std::min(deltaSeconds, MaximumCatchUp);
    while (accumulator_ + 1e-12 >= FixedDelta)
    {
        if (!update_(FixedDelta)) return false;
        if (!triggers_.Tick().empty()) return false;
        accumulator_ -= FixedDelta;
        ++tickCount_;
    }
    return true;
}

void RuntimeGameplaySession::Reset() noexcept
{
    accumulator_ = 0;
    input_.Clear();
    triggers_.Reset();
}

RuntimeGameplayOwnershipCounters
RuntimeGameplaySession::LiveOwnershipCounters() const
{
    auto counters = liveOwnershipQuery_();
    counters.activeTriggerOverlaps = triggers_.OverlapCount();
    return counters;
}

} // namespace SagaRuntimeApp
