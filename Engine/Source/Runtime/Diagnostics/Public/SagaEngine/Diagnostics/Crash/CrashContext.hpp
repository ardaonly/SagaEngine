/// @file CrashContext.hpp
/// @brief Defines caller-supplied context for crash and reliability reports.

#pragma once

#include <map>
#include <optional>
#include <string>

namespace SagaEngine::Diagnostics
{

namespace CrashReportKinds {

inline constexpr const char* ManualCrashReport = "manual_crash_report";
inline constexpr const char* FatalErrorReport = "fatal_error_report";
inline constexpr const char* ReliabilityFailureReport =
    "reliability_failure_report";
inline constexpr const char* SlowTickReport = "slow_tick_report";

} // namespace CrashReportKinds

/// Caller-provided reason and metadata for a crash or reliability report.
struct CrashContext
{
    std::string reportKind = CrashReportKinds::ManualCrashReport; ///< Stable report kind string.
    std::string reason;                                           ///< Human-readable report reason.
    std::optional<std::string> threadId;                          ///< Optional captured thread id.
    std::map<std::string, std::string> metadata;                  ///< Sorted report context fields.
};

} // namespace SagaEngine::Diagnostics
