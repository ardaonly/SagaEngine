/// @file HealthMetric.hpp
/// @brief Defines runtime health metric records used by diagnostics snapshots.

#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Storage model for a health metric value.
enum class HealthMetricType : std::uint8_t
{
    Gauge,
    Counter,
    Timing,
};

/// Named runtime health value captured by HealthMonitor.
struct HealthMetric
{
    std::string name;                                      ///< Stable metric key, such as server.tick.ms.
    HealthMetricType type = HealthMetricType::Gauge;       ///< Interpretation of value.
    double value = 0.0;                                    ///< Numeric value at the latest update.
    std::chrono::system_clock::time_point updatedAt;       ///< Wall-clock time of the latest update.
};

} // namespace SagaEngine::Diagnostics
