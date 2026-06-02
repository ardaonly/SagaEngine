/// @file ChaosReport.hpp
/// @brief Defines SagaChaosLab report result contracts.

#pragma once

#include "SagaChaosLab/ChaosProfile.hpp"
#include "SagaStressArena/ScenarioRunner.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace SagaChaosLab
{

/// Stable SagaChaosLab run status.
enum class ChaosRunStatus
{
    Succeeded,
    InvalidProfile,
    ManualOnlyBlocked,
    ScenarioFailed,
    ReportWriteFailed,
};

/// Result from executing a chaos profile.
struct ChaosRunResult
{
    ChaosRunStatus status = ChaosRunStatus::InvalidProfile;
    int exitCode = 1;
    std::string message;
    ChaosProfile profile;
    std::vector<std::string> diagnostics;
    std::filesystem::path chaosReportPath;
    SagaStressArena::ScenarioRunResult scenarioResult;
    std::map<std::string, double> chaosMetrics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == ChaosRunStatus::Succeeded;
    }
};

/// Convert a chaos run status to a stable JSON/CLI token.
[[nodiscard]] const char* ToString(ChaosRunStatus status) noexcept;

} // namespace SagaChaosLab
