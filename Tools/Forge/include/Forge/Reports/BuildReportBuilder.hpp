/// @file BuildReportBuilder.hpp
/// @brief Creates Forge build reports from planned pipeline metadata.

#pragma once

#include "Forge/Pipeline/BuildPlan.hpp"
#include "Forge/Reports/BuildReport.hpp"

namespace Forge::Reports
{

class BuildReportBuilder
{
public:
    [[nodiscard]] static BuildReport CreatePlannedReport(
        const BuildPlan& plan,
        const BuildPlanValidationResult& validation);
};

} // namespace Forge::Reports
