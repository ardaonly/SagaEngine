// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Authoring/BehaviorAuthoringCapabilities.h"

#include <gtest/gtest.h>

namespace SagaEditor::Authoring
{

TEST(BehaviorAuthoringCapabilitiesTests, CSharpRoutesAllCurrentBehaviorViews)
{
    const auto capabilities = ResolveBehaviorAuthoringCapabilities({
        "behavior://sample/controller", "csharp", "Sample.Controller",
        "Scripts/Controller.cs"});
    EXPECT_TRUE(capabilities.available);
    EXPECT_TRUE(capabilities.sourceEditing);
    EXPECT_TRUE(capabilities.diagnostics);
    EXPECT_TRUE(capabilities.navigation);
    EXPECT_TRUE(capabilities.blocksProjection);
    EXPECT_TRUE(capabilities.controlledBlockEditing);
}

TEST(BehaviorAuthoringCapabilitiesTests, UnknownLanguageIsUnavailableWithoutFallback)
{
    const auto capabilities = ResolveBehaviorAuthoringCapabilities({
        "behavior://sample/controller", "unknown", "sample.controller",
        "Scripts/controller.unknown"});
    EXPECT_FALSE(capabilities.available);
    EXPECT_FALSE(capabilities.sourceEditing);
    EXPECT_FALSE(capabilities.blocksProjection);
    EXPECT_NE(capabilities.unavailableReason.find("unknown"), std::string::npos);
}

} // namespace SagaEditor::Authoring
