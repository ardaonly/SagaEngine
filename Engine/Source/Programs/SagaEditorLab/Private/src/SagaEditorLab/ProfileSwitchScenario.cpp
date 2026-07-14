/// @file ProfileSwitchScenario.cpp
/// @brief Built-in EditorLab profile switch validation scenario.

#include "SagaEditorLab/Scenario/ProfileSwitchScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioStep> MakeProfileSwitchValidationScenario()
{
    return {
        ScenarioStep::MakeOpenPanel("saga.panel.production_dashboard"),
        ScenarioStep::MakeAssertion("editor.engine_bridge.state", "Ready"),
        ScenarioStep::MakeAssertion("editor.customization.boundary", "editor_only"),
        ScenarioStep::MakeAction("saga.command.profile.basic"),
        ScenarioStep::MakeSnapshot("profile.basic"),
        ScenarioStep::MakeAction("saga.command.profile.advanced_pipeline"),
        ScenarioStep::MakeSnapshot("profile.advanced_pipeline"),
        ScenarioStep::MakeAssertion("profile.layout.diff.basic_to_advanced", "true"),
        ScenarioStep::MakeAssertion("profile.shortcuts.diff.basic_to_advanced", "true"),
        ScenarioStep::MakeAssertion("profile.panels.diff.basic_to_advanced", "true"),
        ScenarioStep::MakeAction("saga.command.profile.standard_pipeline"),
        ScenarioStep::MakeSnapshot("profile.standard_pipeline"),
        ScenarioStep::MakeAssertion("profile.layout.diff.advanced_to_standard", "true"),
        ScenarioStep::MakeAssertion("profile.tools.diff.advanced_to_standard", "true"),
        ScenarioStep::MakeAssertion("editor.core.identity.stable", "true"),
        ScenarioStep::MakeAssertion("editor.engine_bridge.identity.stable", "true"),
    };
}

} // namespace SagaEditorLab
