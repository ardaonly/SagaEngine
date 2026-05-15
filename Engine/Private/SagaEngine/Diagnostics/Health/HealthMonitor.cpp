/// @file HealthMonitor.cpp
/// @brief Implements thread-safe runtime health metric storage.

#include <SagaEngine/Diagnostics/Health/HealthMonitor.hpp>

#include <chrono>
#include <utility>

namespace SagaEngine::Diagnostics
{

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
