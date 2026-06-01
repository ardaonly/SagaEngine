/// @file FaultBoundary.cpp
/// @brief Implements minimal fault classification diagnostics.

#include <SagaEngine/Diagnostics/Fault/FaultBoundary.hpp>

#include <algorithm>
#include <utility>

namespace SagaEngine::Diagnostics
{
namespace
{

void SortMetadata(std::vector<std::pair<std::string, std::string>>& metadata)
{
    std::sort(metadata.begin(),
              metadata.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.first != rhs.first)
                  {
                      return lhs.first < rhs.first;
                  }
                  return lhs.second < rhs.second;
              });
}

} // namespace

const char* ToString(FaultSeverity severity) noexcept
{
    switch (severity)
    {
        case FaultSeverity::Info:
            return "Info";
        case FaultSeverity::Warning:
            return "Warning";
        case FaultSeverity::Error:
            return "Error";
        case FaultSeverity::Critical:
            return "Critical";
    }
    return "Unknown";
}

const char* ToString(FaultPolicy policy) noexcept
{
    switch (policy)
    {
        case FaultPolicy::Recoverable:
            return "Recoverable";
        case FaultPolicy::NonRecoverable:
            return "NonRecoverable";
        case FaultPolicy::DeferToCaller:
            return "DeferToCaller";
        case FaultPolicy::ReportOnly:
            return "ReportOnly";
        case FaultPolicy::BlockStartup:
            return "BlockStartup";
    }
    return "Unknown";
}

const char* ToString(RecoveryAction action) noexcept
{
    switch (action)
    {
        case RecoveryAction::None:
            return "None";
        case RecoveryAction::RetryAllowed:
            return "RetryAllowed";
        case RecoveryAction::DisableSubsystem:
            return "DisableSubsystem";
        case RecoveryAction::ShutdownRequested:
            return "ShutdownRequested";
        case RecoveryAction::BlockStartup:
            return "BlockStartup";
        case RecoveryAction::ReportOnly:
            return "ReportOnly";
    }
    return "Unknown";
}

RecoveryAction FaultBoundary::Recommend(FaultPolicy policy) noexcept
{
    switch (policy)
    {
        case FaultPolicy::Recoverable:
            return RecoveryAction::RetryAllowed;
        case FaultPolicy::NonRecoverable:
            return RecoveryAction::ShutdownRequested;
        case FaultPolicy::DeferToCaller:
            return RecoveryAction::None;
        case FaultPolicy::ReportOnly:
            return RecoveryAction::ReportOnly;
        case FaultPolicy::BlockStartup:
            return RecoveryAction::BlockStartup;
    }
    return RecoveryAction::ReportOnly;
}

FaultReport FaultBoundary::BuildReport(
    std::string faultId,
    std::string subsystem,
    FaultSeverity severity,
    FaultPolicy policy,
    std::string message,
    std::string diagnosticCode,
    std::vector<std::pair<std::string, std::string>> metadata)
{
    SortMetadata(metadata);

    FaultReport report;
    report.faultId = std::move(faultId);
    report.subsystem = std::move(subsystem);
    report.severity = severity;
    report.policy = policy;
    report.recommendedAction = Recommend(policy);
    report.message = std::move(message);
    report.diagnosticCode = std::move(diagnosticCode);
    report.metadata = std::move(metadata);
    return report;
}

FaultTracker::FaultTracker(std::size_t maxFaultReports)
    : maxFaultReports_(maxFaultReports)
{
}

void FaultTracker::ConfigureLimit(std::size_t maxFaultReports)
{
    std::lock_guard<std::mutex> lock(mutex_);
    maxFaultReports_ = maxFaultReports;
}

bool FaultTracker::RecordFault(FaultReport report)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (reports_.size() >= maxFaultReports_)
    {
        ++droppedFaultCount_;
        return false;
    }

    SortMetadata(report.metadata);
    if (report.sequence == 0)
    {
        report.sequence = nextSequence_++;
    }
    else if (report.sequence >= nextSequence_)
    {
        nextSequence_ = report.sequence + 1;
    }
    reports_.push_back(std::move(report));
    return true;
}

FaultSnapshot FaultTracker::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    FaultSnapshot snapshot;
    snapshot.reports = reports_;
    std::sort(snapshot.reports.begin(),
              snapshot.reports.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.sequence < rhs.sequence;
              });
    snapshot.summary.faultCount = snapshot.reports.size();
    snapshot.summary.droppedFaultCount = droppedFaultCount_;
    return snapshot;
}

void FaultTracker::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    reports_.clear();
    nextSequence_ = 1;
    droppedFaultCount_ = 0;
}

} // namespace SagaEngine::Diagnostics
