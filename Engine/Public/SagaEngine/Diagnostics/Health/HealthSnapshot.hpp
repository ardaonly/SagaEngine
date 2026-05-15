/// @file HealthSnapshot.hpp
/// @brief Captures an immutable view of diagnostic health metrics.

#pragma once

#include <SagaEngine/Diagnostics/Health/HealthMetric.hpp>

#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

namespace SagaEngine::Diagnostics
{

/// Point-in-time copy of runtime health metrics for reports and tests.
class HealthSnapshot
{
public:
    HealthSnapshot();
    /// Capture the supplied metrics at construction time.
    explicit HealthSnapshot(std::vector<HealthMetric> metrics);

    [[nodiscard]] const std::vector<HealthMetric>& Metrics() const noexcept;
    [[nodiscard]] std::optional<HealthMetric> FindMetric(std::string_view name) const;
    [[nodiscard]] std::chrono::system_clock::time_point CapturedAt() const noexcept;

private:
    std::chrono::system_clock::time_point capturedAt_;
    std::vector<HealthMetric> metrics_;
};

} // namespace SagaEngine::Diagnostics
