/// @file CustomizationScenario.cpp
/// @brief Built-in EditorLab customisation precedence scenario.

#include "SagaEditorLab/Scenario/CustomizationScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioStep> MakeCustomizationPrecedenceScenario()
{
    return {
        ScenarioStep::MakeAssertion("editor.customization.builtin.available", "true"),
        ScenarioStep::MakeAssertion("editor.customization.project.source", "builtin"),
        ScenarioStep::MakeAssertion("editor.customization.user.source", "~/.config/Saga"),
        ScenarioStep::MakeSnapshot("customization.builtin"),
        ScenarioStep::MakeAssertion("editor.customization.project.profiles.loaded", "true"),
        ScenarioStep::MakeAction("saga.command.profile.basic"),
        ScenarioStep::MakeSnapshot("customization.project.basic"),
        ScenarioStep::MakeAction("saga.command.profile.advanced_pipeline"),
        ScenarioStep::MakeSnapshot("customization.project.advanced"),
        ScenarioStep::MakeAssertion("profile.layout.diff.basic_to_advanced", "true"),
        ScenarioStep::MakeAssertion("profile.shortcuts.diff.basic_to_advanced", "true"),
        ScenarioStep::MakeAssertion("editor.customization.user.override.applied", "true"),
        ScenarioStep::MakeAssertion("editor.customization.project.source.unchanged", "true"),
        ScenarioStep::MakeAssertion("editor.core.identity.stable", "true"),
    };
}

} // namespace SagaEditorLab
