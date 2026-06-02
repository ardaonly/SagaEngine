/// @file ScenarioRunner.hpp
/// @brief Runs bounded SagaStressArena diagnostics scenarios.

#pragma once

#include "SagaStressArena/StressReportPaths.hpp"
#include "SagaStressArena/StressScenarioConfig.hpp"

#include <string>
#include <vector>

namespace SagaStressArena
{

/// Deterministic result status for a stress scenario run.
enum class ScenarioRunStatus
{
    Succeeded,
    InvalidConfig,
    ManualTierOnly,
    DurationExceeded,
    ReportWriteFailed,
};

/// Result from running a bounded stress scenario.
struct ScenarioRunResult
{
    ScenarioRunStatus status = ScenarioRunStatus::InvalidConfig; ///< Run status.
    int exitCode = 1;                                             ///< Process exit code.
    std::string message;                                          ///< Stable result message.
    ResolvedStressScenarioConfig config;                          ///< Resolved config.
    StressReportPaths reportPaths;                                ///< Expected report paths.
    std::vector<std::string> diagnostics;                         ///< Run diagnostics.

    /// Return true when the scenario completed and reports were written.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == ScenarioRunStatus::Succeeded;
    }
};

/// Convert a scenario run status to a stable string.
[[nodiscard]] const char* ToString(ScenarioRunStatus status) noexcept;

/// Run bounded direct ZoneServer diagnostics scenarios.
class ScenarioRunner
{
public:
    /// Run the requested scenario with resolved tier bounds.
    [[nodiscard]] static ScenarioRunResult Run(
        const StressScenarioConfig& config);
};

} // namespace SagaStressArena
