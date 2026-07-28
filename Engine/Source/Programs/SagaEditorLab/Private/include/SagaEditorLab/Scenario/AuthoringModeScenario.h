// SPDX-License-Identifier: MPL-2.0
/// @file AuthoringModeScenario.h
/// @brief EditorLab scenario for validating authoring-mode presentation.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioStep.h"

#include <vector>

namespace SagaEditorLab
{

/// Switches the three built-in authoring modes while core services stay stable.
[[nodiscard]] std::vector<ScenarioStep> MakeAuthoringModeValidationScenario();

} // namespace SagaEditorLab
