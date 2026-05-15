/// @file TelemetryAlertSystem.cpp
/// @file TelemetryAlertSystem.cpp
/// @brief Threshold-based alerting for replication telemetry.

#include "SagaEngine/Client/Replication/TelemetryAlertSystem.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Register alert ─────────────────────────────────────────────────────────

int TelemetryAlertSystem::RegisterAlert(AlertRule rule) noexcept
{
    if (ruleCount_ >= kMaxAlertRules || !rule.name)
        return -1;

    std::size_t idx = ruleCount_++;
    rules_[idx] = std::move(rule);
    return static_cast<int>(idx);
}

// ─── Tick ───────────────────────────────────────────────────────────────────

void TelemetryAlertSystem::Tick(std::uint64_t currentTick,
                                 const ReplicationTelemetry& telemetry) noexcept
{
    for (std::size_t i = 0; i < ruleCount_; ++i)
    {
        const AlertRule& rule = rules_[i];
        AlertState& state = states_[i];

        // Determine the metric value to check.
        // The threshold is compared against a specific telemetry field.
        // We use the totalDesyncs as a default metric; production rules
        // would use specific fields.
        std::uint64_t currentValue = telemetry.totalDesyncs;

        bool breached = currentValue >= rule.threshold;

        if (breached && !state.active)
        {
            // Check cool-down.
            if (currentTick - state.lastBreachedAt >= rule.coolDownTicks)
            {
                state.active = true;
                state.lastBreachedAt = currentTick;
                state.fireCount++;

                if (rule.onBreach)
                    rule.onBreach(telemetry);
            }
        }
        else if (!breached && state.active)
        {
            state.active = false;
        }
    }
}

// ─── Active count ───────────────────────────────────────────────────────────

std::uint32_t TelemetryAlertSystem::ActiveAlertCount() const noexcept
{
    std::uint32_t count = 0;
    for (std::size_t i = 0; i < ruleCount_; ++i)
    {
        if (states_[i].active)
            ++count;
    }
    return count;
}

void TelemetryAlertSystem::Reset() noexcept
{
    for (std::size_t i = 0; i < ruleCount_; ++i)
        states_[i] = {};
    ruleCount_ = 0;
}

} // namespace SagaEngine::Client::Replication
