// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/VisualBlocks/BehaviorProjectionRoute.h"

#include <gtest/gtest.h>

namespace SagaEditor::VisualBlocks
{

TEST(BehaviorProjectionRouteTests, RoutesOnlyCanonicalCSharpSourceProjection)
{
    const auto csharp = ResolveBehaviorProjection(
        {"behavior://sample/controller", "csharp"});
    EXPECT_TRUE(csharp.available);
    EXPECT_EQ(csharp.backendId, "csharp-source-projection-v1");
    EXPECT_TRUE(csharp.controlledEditing);

    const auto unknown = ResolveBehaviorProjection(
        {"behavior://sample/controller", "unknown"});
    EXPECT_FALSE(unknown.available);
    EXPECT_TRUE(unknown.backendId.empty());
}

TEST(BehaviorProjectionRouteTests, RejectsLegacyTypeIdentityAsCanonicalModule)
{
    const auto legacy = ResolveBehaviorProjection({"Sample.Controller", "csharp"});
    EXPECT_FALSE(legacy.available);
}

} // namespace SagaEditor::VisualBlocks
