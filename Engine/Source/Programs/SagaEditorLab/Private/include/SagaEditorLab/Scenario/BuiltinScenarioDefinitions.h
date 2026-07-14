/// @file BuiltinScenarioDefinitions.h
/// @brief Built-in EditorLab scenarios exposed to the Scenario Runner panel.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioDefinition.h"

#include <vector>

namespace SagaEditorLab
{

/// Return deterministic built-in scenarios available in the lab UI.
[[nodiscard]] std::vector<ScenarioDefinition> MakeBuiltinScenarioDefinitions();

} // namespace SagaEditorLab
