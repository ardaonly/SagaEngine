/// @file FaultBoundary.hpp
/// @brief Minimal runtime fault policy classification helpers.

#pragma once

#include <SagaEngine/Diagnostics/Fault/FaultReport.hpp>

#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Diagnostics
{

class FaultBoundary
{
public:
    [[nodiscard]] static RecoveryAction Recommend(
        FaultPolicy policy) noexcept;

    [[nodiscard]] static FaultReport BuildReport(
        std::string faultId,
        std::string subsystem,
        FaultSeverity severity,
        FaultPolicy policy,
        std::string message,
        std::string diagnosticCode = {},
        std::vector<std::pair<std::string, std::string>> metadata = {});
};

class FaultTracker
{
public:
    FaultTracker() = default;
    explicit FaultTracker(std::size_t maxFaultReports);

    void ConfigureLimit(std::size_t maxFaultReports);

    [[nodiscard]] bool RecordFault(FaultReport report);
    [[nodiscard]] FaultSnapshot Snapshot() const;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::vector<FaultReport> reports_;
    std::uint64_t nextSequence_ = 1;
    std::size_t maxFaultReports_ = 128;
    std::size_t droppedFaultCount_ = 0;
};

} // namespace SagaEngine::Diagnostics
