/// @file FaultReport.hpp
/// @brief Minimal runtime fault classification diagnostics contracts.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Diagnostics
{

enum class FaultSeverity
{
    Info = 0,
    Warning,
    Error,
    Critical,
};

enum class FaultPolicy
{
    Recoverable = 0,
    NonRecoverable,
    DeferToCaller,
    ReportOnly,
    BlockStartup,
};

enum class RecoveryAction
{
    None = 0,
    RetryAllowed,
    DisableSubsystem,
    ShutdownRequested,
    BlockStartup,
    ReportOnly,
};

struct FaultReport
{
    std::uint64_t sequence = 0;
    std::string faultId;
    std::string subsystem;
    FaultSeverity severity = FaultSeverity::Error;
    FaultPolicy policy = FaultPolicy::ReportOnly;
    RecoveryAction recommendedAction = RecoveryAction::ReportOnly;
    std::string message;
    std::string diagnosticCode;
    std::vector<std::pair<std::string, std::string>> metadata;
};

struct FaultReportSummary
{
    std::size_t faultCount = 0;
    std::size_t droppedFaultCount = 0;
};

struct FaultSnapshot
{
    std::vector<FaultReport> reports;
    FaultReportSummary summary;
};

[[nodiscard]] const char* ToString(FaultSeverity severity) noexcept;
[[nodiscard]] const char* ToString(FaultPolicy policy) noexcept;
[[nodiscard]] const char* ToString(RecoveryAction action) noexcept;

} // namespace SagaEngine::Diagnostics
