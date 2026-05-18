/// @file BuiltinScenarioDefinitions.cpp
/// @brief Built-in scenario definitions for the EditorLab panel.

#include "SagaEditorLab/Scenario/BuiltinScenarioDefinitions.h"
#include "SagaEditorLab/Scenario/CustomizationScenario.h"
#include "SagaEditorLab/Scenario/ProfileSwitchScenario.h"

namespace SagaEditorLab
{

std::vector<ScenarioDefinition> MakeBuiltinScenarioDefinitions()
{
    return {
        {
            "saga.lab.profile_switch",
            "Profile Switch Validation",
            MakeProfileSwitchValidationScenario(),
        },
        {
            "saga.lab.customization_precedence",
            "Customization Precedence",
            MakeCustomizationPrecedenceScenario(),
        },
    };
}

} // namespace SagaEditorLab
