// SPDX-License-Identifier: MPL-2.0
/// @file BuiltinScenarioDefinitions.cpp
/// @brief Built-in scenario definitions for the EditorLab panel.

#include "SagaEditorLab/Scenario/BuiltinScenarioDefinitions.h"
#include "SagaEditorLab/Scenario/CustomizationScenario.h"
#include "SagaEditorLab/Scenario/AuthoringModeScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioDefinition> MakeBuiltinScenarioDefinitions()
{
    return {
        {
            "saga.lab.authoring_mode",
            "Authoring Mode Validation",
            MakeAuthoringModeValidationScenario(),
        },
        {
            "saga.lab.customization_precedence",
            "Customization Precedence",
            MakeCustomizationPrecedenceScenario(),
        },
    };
}

} // namespace SagaEditorLab
