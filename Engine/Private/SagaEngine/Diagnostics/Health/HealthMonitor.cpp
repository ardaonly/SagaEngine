/// @file HealthMonitor.cpp
/// @brief Implements thread-safe runtime health metric storage.

#include <SagaEngine/Diagnostics/Health/HealthMonitor.hpp>

#include <algorithm>
#include <chrono>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

[[nodiscard]] HealthMetricType ExpectedMetricType(HealthRuleType type) noexcept
{
    switch (type)
    {
        case HealthRuleType::GaugeGreaterThan:
        case HealthRuleType::GaugeLessThan:
            return HealthMetricType::Gauge;
        case HealthRuleType::CounterGreaterThan:
            return HealthMetricType::Counter;
        case HealthRuleType::TimingGreaterThan:
            return HealthMetricType::Timing;
    }
    return HealthMetricType::Gauge;
}

[[nodiscard]] bool RulePasses(const HealthRule& rule, double value) noexcept
{
    switch (rule.type)
    {
        case HealthRuleType::GaugeGreaterThan:
        case HealthRuleType::CounterGreaterThan:
        case HealthRuleType::TimingGreaterThan:
            return value <= rule.threshold;
        case HealthRuleType::GaugeLessThan:
            return value >= rule.threshold;
    }
    return false;
}

[[nodiscard]] const char* FailureMessage(HealthRuleType type) noexcept
{
    switch (type)
    {
        case HealthRuleType::GaugeGreaterThan:
        case HealthRuleType::CounterGreaterThan:
        case HealthRuleType::TimingGreaterThan:
            return "Threshold exceeded.";
        case HealthRuleType::GaugeLessThan:
            return "Threshold undercut.";
    }
    return "Rule failed.";
}

[[nodiscard]] HealthRuleResult MakeBaseResult(const HealthRule& rule)
{
    HealthRuleResult result;
    result.ruleName = rule.ruleName;
    result.metricName = rule.metricName;
    result.type = rule.type;
    result.severity = rule.severity;
    result.threshold = rule.threshold;
    return result;
}

} // namespace

void HealthMonitor::SetGauge(std::string name, double value)
{
    SetMetric(std::move(name), HealthMetricType::Gauge, value);
}

void HealthMonitor::IncrementCounter(std::string name, double amount)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto key = std::move(name);
    auto& metric = metrics_[key];
    if (metric.name.empty())
    {
        metric.name = key;
    }
    metric.type = HealthMetricType::Counter;
    metric.value += amount;
    metric.updatedAt = std::chrono::system_clock::now();
}

void HealthMonitor::RecordTiming(std::string name, double milliseconds)
{
    SetMetric(std::move(name), HealthMetricType::Timing, milliseconds);
}

void HealthMonitor::RegisterRule(HealthRule rule)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto key = rule.ruleName;
    rules_[key] = std::move(rule);
}

void HealthMonitor::ClearRules()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.clear();
}

HealthSnapshot HealthMonitor::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HealthMetric> metrics;
    metrics.reserve(metrics_.size());
    for (const auto& [_, metric] : metrics_)
    {
        metrics.push_back(metric);
    }
    return HealthSnapshot(std::move(metrics));
}

std::vector<HealthRule> HealthMonitor::SnapshotRules() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HealthRule> rules;
    rules.reserve(rules_.size());
    for (const auto& [_, rule] : rules_)
    {
        rules.push_back(rule);
    }
    std::sort(rules.begin(),
              rules.end(),
              [](const HealthRule& lhs, const HealthRule& rhs) {
                  if (lhs.ruleName != rhs.ruleName)
                  {
                      return lhs.ruleName < rhs.ruleName;
                  }
                  return lhs.metricName < rhs.metricName;
              });
    return rules;
}

std::vector<HealthRuleResult> HealthMonitor::EvaluateRules(
    const HealthSnapshot& snapshot) const
{
    const auto rules = SnapshotRules();
    std::vector<HealthRuleResult> results;
    results.reserve(rules.size());

    for (const auto& rule : rules)
    {
        auto result = MakeBaseResult(rule);
        const auto metric = snapshot.FindMetric(rule.metricName);
        if (!metric.has_value())
        {
            result.message = "Metric not present.";
            results.push_back(std::move(result));
            continue;
        }

        if (metric->type != ExpectedMetricType(rule.type))
        {
            result.message = "Metric type mismatch.";
            results.push_back(std::move(result));
            continue;
        }

        result.evaluated = true;
        result.observedValue = metric->value;
        result.passed = RulePasses(rule, metric->value);
        result.message = result.passed ? "Rule passed." : FailureMessage(rule.type);
        results.push_back(std::move(result));
    }

    return results;
}

bool HealthMonitor::HasMetric(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_.find(std::string(name)) != metrics_.end();
}

void HealthMonitor::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

void HealthMonitor::SetMetric(std::string name, HealthMetricType type, double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto key = std::move(name);
    auto& metric = metrics_[key];
    metric.name = key;
    metric.type = type;
    metric.value = value;
    metric.updatedAt = std::chrono::system_clock::now();
}

} // namespace SagaEngine::Diagnostics
