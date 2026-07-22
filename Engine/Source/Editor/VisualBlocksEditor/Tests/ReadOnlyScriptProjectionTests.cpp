// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h"

#include <gtest/gtest.h>

TEST(ReadOnlyScriptProjectionTests, PreservesHierarchyAndSourceIdentity)
{
    using namespace SagaEditor::VisualBlocks;
    const ProjectionSourceSpan source{"Scripts/DoorController.cs", "sha256:source", 40, 80, 3, 1, 6, 2};
    ProjectionCard child{"state://door", "StateMutation", "toggle door target state",
        "generation", source};
    ProjectionCard root{"action://Interact", "Event", "WHEN Interact", "generation",
        source, {child}};
    const ReadOnlyScriptProjection projection{"starter-arena", "sha256:project",
        "sha256:revision", "generation", "sha256:generation", "sha256:artifact",
        {root}, {}};

    ASSERT_EQ(projection.Roots().size(), 1u);
    ASSERT_EQ(projection.Roots()[0].Children().size(), 1u);
    EXPECT_EQ(projection.Roots()[0].DisplayName(), "WHEN Interact");
    EXPECT_EQ(projection.Roots()[0].Children()[0].DisplayName(), "toggle door target state");
    EXPECT_EQ(projection.Roots()[0].Children()[0].Source(), source);
    EXPECT_EQ(projection.PackageGenerationId(), "generation");
}
