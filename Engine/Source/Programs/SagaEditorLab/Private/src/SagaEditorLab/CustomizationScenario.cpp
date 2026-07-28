// SPDX-License-Identifier: MPL-2.0
/// @file CustomizationScenario.cpp
/// @brief Built-in EditorLab customisation precedence scenario.

#include "SagaEditorLab/Scenario/CustomizationScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioStep> MakeCustomizationPrecedenceScenario()
{
    return {
        ScenarioStep::MakeAssertion("editor.authoring.modes.available", "true"),
        ScenarioStep::MakeAssertion("editor.customization.project.source", "builtin"),
        ScenarioStep::MakeAssertion("editor.customization.user.source", "~/.config/Saga"),
        ScenarioStep::MakeSnapshot("customization.builtin"),
        ScenarioStep::MakeAction("saga.command.authoring.level1"),
        ScenarioStep::MakeSnapshot("customization.authoring.level1"),
        ScenarioStep::MakeAction("saga.command.authoring.level3"),
        ScenarioStep::MakeSnapshot("customization.authoring.level3"),
        ScenarioStep::MakeAssertion(
            "authoring.panels.diff.level1_to_level3", "true"),
        ScenarioStep::MakeAssertion("editor.customization.user.override.applied", "true"),
        ScenarioStep::MakeAssertion("editor.customization.project.source.unchanged", "true"),
        ScenarioStep::MakeAssertion("editor.core.identity.stable", "true"),
    };
}

} // namespace SagaEditorLab
