/// @file HealthSnapshot.cpp
/// @brief Implements immutable runtime health metric snapshots.

#include <SagaEngine/Diagnostics/Health/HealthSnapshot.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{

HealthSnapshot::HealthSnapshot()
    : capturedAt_(std::chrono::system_clock::now())
{
}

HealthSnapshot::HealthSnapshot(std::vector<HealthMetric> metrics)
    : capturedAt_(std::chrono::system_clock::now())
    , metrics_(std::move(metrics))
{
    std::sort(metrics_.begin(),
              metrics_.end(),
              [](const HealthMetric& lhs, const HealthMetric& rhs) {
                  return lhs.name < rhs.name;
              });
}

const std::vector<HealthMetric>& HealthSnapshot::Metrics() const noexcept
{
    return metrics_;
}

std::optional<HealthMetric> HealthSnapshot::FindMetric(std::string_view name) const
{
    for (const auto& metric : metrics_)
    {
        if (metric.name == name)
        {
            return metric;
        }
    }
    return std::nullopt;
}

std::chrono::system_clock::time_point HealthSnapshot::CapturedAt() const noexcept
{
    return capturedAt_;
}

} // namespace SagaEngine::Diagnostics
