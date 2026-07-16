// SPDX-License-Identifier: MPL-2.0
/// @file EditorLabBuiltinScenarioTests.cpp
/// @brief Owner-local contracts for the built-in EditorLab scenarios.

#include "SagaEditorLab/Scenario/CustomizationScenario.h"
#include "SagaEditorLab/Scenario/ProfileSwitchScenario.h"

#include <gtest/gtest.h>

namespace
{

using SagaEditorLab::MakeCustomizationPrecedenceScenario;
using SagaEditorLab::MakeProfileSwitchValidationScenario;

TEST(EditorLabProfileScenarioTest, BuiltinScenarioCapturesVisibleProfileDiffs)
{
    const auto steps = MakeProfileSwitchValidationScenario();
    ASSERT_GE(steps.size(), 14u);

    EXPECT_EQ(steps[0].panel.panelId, "saga.panel.production_dashboard");
    EXPECT_EQ(steps[1].assertion.statePath, "editor.engine_bridge.state");
    EXPECT_EQ(steps[3].action.commandId, "saga.command.profile.basic");
    EXPECT_EQ(steps[5].action.commandId, "saga.command.profile.advanced_pipeline");
    EXPECT_EQ(steps[10].action.commandId, "saga.command.profile.standard_pipeline");
    EXPECT_EQ(steps[7].assertion.statePath, "profile.layout.diff.basic_to_advanced");
    EXPECT_EQ(steps[8].assertion.statePath, "profile.shortcuts.diff.basic_to_advanced");
    EXPECT_EQ(steps[9].assertion.statePath, "profile.panels.diff.basic_to_advanced");
    EXPECT_EQ(steps[steps.size() - 2].assertion.statePath, "editor.core.identity.stable");
    EXPECT_EQ(steps.back().assertion.statePath, "editor.engine_bridge.identity.stable");
}

TEST(EditorLabCustomizationScenarioTest, CapturesProjectAndUserPrecedence)
{
    const auto steps = MakeCustomizationPrecedenceScenario();
    ASSERT_GE(steps.size(), 12u);

    EXPECT_EQ(steps[0].assertion.statePath,
              "editor.customization.builtin.available");
    EXPECT_EQ(steps[1].assertion.expectedValue, "builtin");
    EXPECT_EQ(steps[2].assertion.expectedValue, "~/.config/Saga");
    EXPECT_EQ(steps[4].assertion.statePath,
              "editor.customization.project.profiles.loaded");
    EXPECT_EQ(steps[5].action.commandId, "saga.command.profile.basic");
    EXPECT_EQ(steps[7].action.commandId,
              "saga.command.profile.advanced_pipeline");
    EXPECT_EQ(steps[11].assertion.statePath,
              "editor.customization.user.override.applied");
    EXPECT_EQ(steps.back().assertion.statePath,
              "editor.core.identity.stable");
}

} // namespace
