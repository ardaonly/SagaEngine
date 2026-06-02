/// @file StressReportPaths.hpp
/// @brief Defines deterministic SagaStressArena report artifact paths.

#pragma once

#include <filesystem>

namespace SagaStressArena
{

/// Report artifact paths produced by one stress arena run.
struct StressReportPaths
{
    std::filesystem::path operationalSnapshot; ///< Operational diagnostics report.
    std::filesystem::path reliabilityReport;   ///< Reliability diagnostics report.
    std::filesystem::path lifetimeLeaks;       ///< Lifetime leak diagnostics report.
};

/// Build deterministic report artifact paths under a report directory.
[[nodiscard]] StressReportPaths BuildStressReportPaths(
    const std::filesystem::path& reportDirectory);

} // namespace SagaStressArena
