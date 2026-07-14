/// @file ScenarioDefinition.h
/// @brief Serializable identity and step list for an EditorLab scenario.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioStep.h"

#include <string>
#include <vector>

namespace SagaEditorLab
{

/// Scenario metadata plus ordered steps consumed by ScenarioRunner.
struct ScenarioDefinition
{
    std::string id;                 ///< Stable scenario id used by reports.
    std::string name;               ///< Human-readable scenario name.
    std::vector<ScenarioStep> steps; ///< Ordered scenario operations.
};

} // namespace SagaEditorLab
