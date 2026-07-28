// SPDX-License-Identifier: MPL-2.0
/// @file EditorLabBuiltinScenarioTests.cpp
/// @brief Owner-local contracts for the built-in EditorLab scenarios.

#include "SagaEditorLab/Scenario/AuthoringModeScenario.h"
#include "SagaEditorLab/Scenario/CustomizationScenario.h"

#include <gtest/gtest.h>

namespace
{

using SagaEditorLab::MakeAuthoringModeValidationScenario;
using SagaEditorLab::MakeCustomizationPrecedenceScenario;

TEST(EditorLabAuthoringModeScenarioTest, BuiltinScenarioCapturesModePresentationDiffs)
{
    const auto steps = MakeAuthoringModeValidationScenario();
    ASSERT_GE(steps.size(), 12u);

    EXPECT_EQ(steps[0].panel.panelId, "saga.panel.viewport");
    EXPECT_EQ(steps[1].assertion.statePath, "editor.engine_bridge.state");
    EXPECT_EQ(steps[3].action.commandId, "saga.command.authoring.level1");
    EXPECT_EQ(steps[5].action.commandId, "saga.command.authoring.level3");
    EXPECT_EQ(steps[8].action.commandId, "saga.command.authoring.level2");
    EXPECT_EQ(steps[7].assertion.statePath,
              "authoring.panels.diff.level1_to_level3");
    EXPECT_EQ(steps[10].assertion.statePath,
              "authoring.panels.diff.level3_to_level2");
    EXPECT_EQ(steps[steps.size() - 2].assertion.statePath, "editor.core.identity.stable");
    EXPECT_EQ(steps.back().assertion.statePath, "editor.engine_bridge.identity.stable");
}

TEST(EditorLabCustomizationScenarioTest, CapturesProjectAndUserPrecedence)
{
    const auto steps = MakeCustomizationPrecedenceScenario();
    ASSERT_GE(steps.size(), 12u);

    EXPECT_EQ(steps[0].assertion.statePath,
              "editor.authoring.modes.available");
    EXPECT_EQ(steps[1].assertion.expectedValue, "builtin");
    EXPECT_EQ(steps[2].assertion.expectedValue, "~/.config/Saga");
    EXPECT_EQ(steps[4].action.commandId, "saga.command.authoring.level1");
    EXPECT_EQ(steps[6].action.commandId,
              "saga.command.authoring.level3");
    EXPECT_EQ(steps[9].assertion.statePath,
              "editor.customization.user.override.applied");
    EXPECT_EQ(steps.back().assertion.statePath,
              "editor.core.identity.stable");
}

} // namespace
