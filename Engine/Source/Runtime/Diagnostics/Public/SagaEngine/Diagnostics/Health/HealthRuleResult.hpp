/// @file HealthRuleResult.hpp
/// @brief Defines deterministic health rule evaluation results.

#pragma once

#include <SagaEngine/Diagnostics/Health/HealthRule.hpp>

#include <string>

namespace SagaEngine::Diagnostics
{

/// Result produced by evaluating one health rule against a snapshot.
struct HealthRuleResult
{
    std::string ruleName;             ///< Stable rule id used for sorting and reports.
    std::string metricName;           ///< Health metric key evaluated by this rule.
    HealthRuleType type = HealthRuleType::GaugeGreaterThan; ///< Evaluated comparison kind.
    HealthSeverity severity = HealthSeverity::Warning;      ///< Severity when the rule fails.
    double threshold = 0.0;           ///< Configured threshold.
    double observedValue = 0.0;       ///< Metric value when evaluation occurred.
    bool evaluated = false;           ///< False when the metric is missing or has the wrong type.
    bool passed = false;              ///< True only when the metric was evaluated and passed.
    std::string message;              ///< Stable human-readable result detail.
};

} // namespace SagaEngine::Diagnostics
