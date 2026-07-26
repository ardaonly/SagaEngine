// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/ScriptProjectionSession.h"

#include <gtest/gtest.h>

namespace
{
SagaEditor::ScriptProjectionRequestIdentity Identity(std::string operation = "operation")
{ return {7, "sha256:disk", 9, "sha256:project", "generation", "sha256:generation", std::move(operation)}; }

std::shared_ptr<const SagaEditor::VisualBlocks::ReadOnlyScriptProjection> Projection()
{
    using SagaEditor::VisualBlocks::ReadOnlyScriptProjection;
    return std::make_shared<const ReadOnlyScriptProjection>("project", "sha256:project",
        "sha256:source", "generation", "sha256:generation", "sha256:artifact",
        std::vector<SagaEditor::VisualBlocks::ProjectionCard>{},
        std::vector<SagaEditor::VisualBlocks::ProjectionCard>{});
}
}

TEST(ScriptProjectionSessionTests, PublishesOnlyExactAnalyzingIdentity)
{
    SagaEditor::ScriptProjectionSession session;
    session.Begin(Identity());
    EXPECT_FALSE(session.Publish(Identity("stale"), "sha256:artifact", Projection()));
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::Stale);
    session.Begin(Identity());
    EXPECT_TRUE(session.Publish(Identity(), "sha256:artifact", Projection()));
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::Current);
}

TEST(ScriptProjectionSessionTests, DirtyCloseCancelAndPackageReplacementInvalidatePublication)
{
    SagaEditor::ScriptProjectionSession session;
    session.Begin(Identity());
    session.Cancel();
    EXPECT_FALSE(session.Publish(Identity(), "sha256:artifact", Projection()));
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::Cancelled);
    session.Begin(Identity());
    session.PackageGenerationChanged("replacement", "sha256:replacement");
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::PackageGenerationChanged);
    session.ProjectClosed();
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::ProjectClosed);
    session.Dirty();
    EXPECT_EQ(session.Snapshot().state, SagaEditor::ScriptProjectionState::DirtyNeedsSave);
}
