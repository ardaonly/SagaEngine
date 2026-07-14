/// @file CustomizationScenario.h
/// @brief EditorLab scenarios for project/user editor customisation precedence.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioStep.h"

#include <vector>

namespace SagaEditorLab
{

/// Validates the professional editor customisation stack:
/// built-ins < user-private overrides.
[[nodiscard]] std::vector<ScenarioStep> MakeCustomizationPrecedenceScenario();

} // namespace SagaEditorLab
