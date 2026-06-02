/// @file ChaosReport.cpp
/// @brief Implements SagaChaosLab stable status strings.

#include "SagaChaosLab/ChaosReport.hpp"

namespace SagaChaosLab
{

const char* ToString(ChaosRunStatus status) noexcept
{
    switch (status)
    {
        case ChaosRunStatus::Succeeded:
            return "succeeded";
        case ChaosRunStatus::InvalidProfile:
            return "invalid_profile";
        case ChaosRunStatus::ManualOnlyBlocked:
            return "manual_only_blocked";
        case ChaosRunStatus::ScenarioFailed:
            return "scenario_failed";
        case ChaosRunStatus::ReportWriteFailed:
            return "report_write_failed";
    }
    return "unknown";
}

} // namespace SagaChaosLab
