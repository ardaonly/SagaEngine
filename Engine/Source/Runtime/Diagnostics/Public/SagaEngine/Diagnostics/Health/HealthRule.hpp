/// @file HealthRule.hpp
/// @brief Defines threshold rules evaluated against health snapshots.

#pragma once

#include <SagaEngine/Diagnostics/Health/HealthSeverity.hpp>

#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Comparison and expected metric type used by a health rule.
enum class HealthRuleType : std::uint8_t
{
    GaugeGreaterThan,
    GaugeLessThan,
    CounterGreaterThan,
    TimingGreaterThan,
};

/// Deterministic threshold rule for one named health metric.
struct HealthRule
{
    std::string ruleName;                         ///< Stable rule id used for sorting and reports.
    std::string metricName;                       ///< Health metric key evaluated by this rule.
    HealthRuleType type = HealthRuleType::GaugeGreaterThan; ///< Threshold comparison kind.
    double threshold = 0.0;                       ///< Numeric threshold for the comparison.
    HealthSeverity severity = HealthSeverity::Warning; ///< Severity when the rule fails.
};

/// Convert a health rule type to its stable report string.
[[nodiscard]] const char* ToString(HealthRuleType type) noexcept;

} // namespace SagaEngine::Diagnostics
