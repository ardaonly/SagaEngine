// SPDX-License-Identifier: MPL-2.0
/// @file AuthoringModeScenario.cpp
/// @brief Built-in EditorLab authoring-mode validation scenario.

#include "SagaEditorLab/Scenario/AuthoringModeScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioStep> MakeAuthoringModeValidationScenario()
{
    return {
        ScenarioStep::MakeOpenPanel("saga.panel.viewport"),
        ScenarioStep::MakeAssertion("editor.engine_bridge.state", "Ready"),
        ScenarioStep::MakeAssertion("editor.customization.boundary", "editor_only"),
        ScenarioStep::MakeAction("saga.command.authoring.level1"),
        ScenarioStep::MakeSnapshot("authoring.level1"),
        ScenarioStep::MakeAction("saga.command.authoring.level3"),
        ScenarioStep::MakeSnapshot("authoring.level3"),
        ScenarioStep::MakeAssertion(
            "authoring.panels.diff.level1_to_level3", "true"),
        ScenarioStep::MakeAction("saga.command.authoring.level2"),
        ScenarioStep::MakeSnapshot("authoring.level2"),
        ScenarioStep::MakeAssertion(
            "authoring.panels.diff.level3_to_level2", "true"),
        ScenarioStep::MakeAssertion("editor.core.identity.stable", "true"),
        ScenarioStep::MakeAssertion("editor.engine_bridge.identity.stable", "true"),
    };
}

} // namespace SagaEditorLab
