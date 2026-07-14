/// @file ProfileSwitchScenario.h
/// @brief EditorLab scenario for validating visible workflow profile changes.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioStep.h"

#include <vector>

namespace SagaEditorLab
{

/// Builds the first product-validation scenario for editor workflow profiles.
/// The runner opens the editor shell, executes these profile switch actions,
/// captures snapshots, and asserts that layout, shortcut, panel, and tool
/// exposure differ while editor core services stay stable.
[[nodiscard]] std::vector<ScenarioStep> MakeProfileSwitchValidationScenario();

} // namespace SagaEditorLab
