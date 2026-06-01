/// @file HealthRule.cpp
/// @brief Implements stable health rule string conversions.

#include <SagaEngine/Diagnostics/Health/HealthRule.hpp>

namespace SagaEngine::Diagnostics
{

const char* ToString(HealthSeverity severity) noexcept
{
    switch (severity)
    {
        case HealthSeverity::Info: return "info";
        case HealthSeverity::Warning: return "warning";
        case HealthSeverity::Error: return "error";
        case HealthSeverity::Critical: return "critical";
    }
    return "unknown";
}

const char* ToString(HealthRuleType type) noexcept
{
    switch (type)
    {
        case HealthRuleType::GaugeGreaterThan: return "gauge_greater_than";
        case HealthRuleType::GaugeLessThan: return "gauge_less_than";
        case HealthRuleType::CounterGreaterThan: return "counter_greater_than";
        case HealthRuleType::TimingGreaterThan: return "timing_greater_than";
    }
    return "unknown";
}

} // namespace SagaEngine::Diagnostics
