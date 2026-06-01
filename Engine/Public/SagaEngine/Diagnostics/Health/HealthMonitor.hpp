/// @file HealthMonitor.hpp
/// @brief Tracks runtime health counters, gauges, and timings.

#pragma once

#include <SagaEngine/Diagnostics/Health/HealthRuleResult.hpp>
#include <SagaEngine/Diagnostics/Health/HealthSnapshot.hpp>

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace SagaEngine::Diagnostics
{

/// Thread-safe in-process metric registry for server health snapshots.
class HealthMonitor
{
public:
    /// Set the current value of a gauge metric.
    void SetGauge(std::string name, double value);
    /// Increase a counter metric by the supplied amount.
    void IncrementCounter(std::string name, double amount = 1.0);
    /// Record the latest duration for a timing metric.
    void RecordTiming(std::string name, double milliseconds);

    /// Register or replace a deterministic health rule.
    void RegisterRule(HealthRule rule);
    /// Remove all registered health rules.
    void ClearRules();

    /// Return an immutable copy of all current metrics.
    [[nodiscard]] HealthSnapshot Snapshot() const;
    /// Return a deterministic copy of registered rules.
    [[nodiscard]] std::vector<HealthRule> SnapshotRules() const;
    /// Evaluate the registered rules against the supplied snapshot.
    [[nodiscard]] std::vector<HealthRuleResult> EvaluateRules(
        const HealthSnapshot& snapshot) const;
    [[nodiscard]] bool HasMetric(std::string_view name) const;
    void Clear();

private:
    void SetMetric(std::string name, HealthMetricType type, double value);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, HealthMetric> metrics_;
    std::unordered_map<std::string, HealthRule> rules_;
};

} // namespace SagaEngine::Diagnostics
