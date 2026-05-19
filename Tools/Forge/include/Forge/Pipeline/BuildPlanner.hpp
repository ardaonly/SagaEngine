/// @file BuildPlanner.hpp
/// @brief Lowers BuildModel into a metadata-only BuildPlan.

#pragma once

#include "Forge/BuildModel.h"
#include "Forge/Pipeline/BuildPlan.hpp"

namespace Forge
{

/// Deterministic planner for the initial Forge pipeline model.
class BuildPlanner
{
public:
    [[nodiscard]] static BuildPlan PlanForCommand(const BuildModel& model,
                                                  BuildPlanCommand command);

    [[nodiscard]] static BuildPlanValidationResult Validate(const BuildPlan& plan);
};

} // namespace Forge
