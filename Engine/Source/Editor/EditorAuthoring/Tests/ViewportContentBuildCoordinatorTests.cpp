// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Viewport/ViewportContentBuildCoordinator.h"

#include <gtest/gtest.h>

#include <chrono>

namespace
{
SagaEditor::ViewportContentBuildRequest Request(std::string operation,
    std::uint64_t workspace = 1, std::string manifest = "sha256:manifest",
    std::string scene = "sha256:scene", std::string assets = "sha256:assets",
    std::string project = "project")
{
    return {std::move(operation), workspace, std::move(project), "/project",
        "/project/Project.sagaproj", std::move(manifest), "scene:/project/main.sagascene",
        std::move(scene), std::move(assets), "generation", "sha256:generation", workspace};
}

SagaEditor::ViewportContentBuildResult Success(
    const SagaEditor::ViewportContentBuildRequest& request, std::string root)
{
    return {request, true, false, std::move(root), {}, {}};
}
}

TEST(ViewportContentBuildCoordinatorTests, StaleBuildCannotPublish)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    auto changed = Request("current", 1, "sha256:manifest", "sha256:new-scene");
    const auto completed = coordinator.Complete(Success(first, "/stale"), changed);
    EXPECT_TRUE(completed.stale);
    EXPECT_FALSE(completed.publish);
    EXPECT_TRUE(coordinator.PublishedRoot().empty());
}

TEST(ViewportContentBuildCoordinatorTests, SceneAndAssetChangesCoalesceToNewestRerun)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    EXPECT_FALSE(coordinator.Request(Request("scene", 2, "sha256:manifest", "sha256:scene-2")));
    const auto newest = Request("assets", 3, "sha256:manifest", "sha256:scene-2", "sha256:assets-3");
    EXPECT_FALSE(coordinator.Request(newest));
    const auto completed = coordinator.Complete(Success(first, "/old"), newest);
    ASSERT_TRUE(completed.nextRequest);
    EXPECT_EQ(*completed.nextRequest, newest);
    EXPECT_TRUE(coordinator.Active());
    EXPECT_FALSE(coordinator.Pending());
}

TEST(ViewportContentBuildCoordinatorTests, ProjectManifestAndWorkspaceChangesRejectOldResult)
{
    for (const auto& changed : {Request("current", 1, "sha256:new-manifest"),
                                Request("current", 2),
                                Request("current", 1, "sha256:manifest", "sha256:scene",
                                    "sha256:assets", "other-project")})
    {
        SagaEditor::ViewportContentBuildCoordinator coordinator;
        const auto first = *coordinator.Request(Request("one"));
        EXPECT_FALSE(coordinator.Complete(Success(first, "/old"), changed).publish);
    }
}

TEST(ViewportContentBuildCoordinatorTests, SameProjectIdWithChangedManifestCannotPublish)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    const auto current = Request("current", 1, "sha256:replacement-manifest");
    EXPECT_TRUE(coordinator.Complete(Success(first, "/old"), current).stale);
}

TEST(ViewportContentBuildCoordinatorTests, ProjectSwitchRejectsOldCompletion)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    const auto current = Request("current", 2, "sha256:manifest", "sha256:scene",
        "sha256:assets", "replacement-project");
    EXPECT_FALSE(coordinator.Complete(Success(first, "/old"), current).publish);
}

TEST(ViewportContentBuildCoordinatorTests, WorkspaceGenerationChangeRejectsOldCompletion)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one", 7));
    EXPECT_TRUE(coordinator.Complete(Success(first, "/old"), Request("current", 8)).stale);
}

TEST(ViewportContentBuildCoordinatorTests, FailedReplacementPreservesValidGeneration)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    ASSERT_TRUE(coordinator.Complete(Success(first, "/valid"), first).publish);
    const auto replacement = *coordinator.Request(Request("two", 2));
    SagaEditor::ViewportContentBuildResult failed{replacement, false, false, "/partial",
        "EditorRender.ContentBuild.Failed", "injected failure"};
    const auto completed = coordinator.Complete(std::move(failed), replacement);
    EXPECT_FALSE(completed.publish);
    EXPECT_EQ(coordinator.PublishedRoot(), "/valid");
}

TEST(ViewportContentBuildCoordinatorTests, CancellationNeverPublishesAndDropsPending)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto first = *coordinator.Request(Request("one"));
    EXPECT_FALSE(coordinator.Request(Request("two", 2)));
    coordinator.Cancel();
    auto completed = coordinator.Complete(Success(first, "/cancelled"), first);
    EXPECT_FALSE(completed.publish);
    EXPECT_FALSE(completed.nextRequest);
    EXPECT_FALSE(coordinator.Pending());
}

TEST(ViewportContentBuildCoordinatorTests, UnknownLateCompletionCannotAccessActivePublication)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    const auto active = *coordinator.Request(Request("active"));
    const auto late = Request("destroyed-widget-operation");
    const auto completed = coordinator.Complete(Success(late, "/late"), active);
    EXPECT_TRUE(completed.stale);
    EXPECT_TRUE(coordinator.Active());
    EXPECT_TRUE(coordinator.PublishedRoot().empty());
}

TEST(ViewportContentBuildCoordinatorTests, CancellationStateTransitionIsBoundedAndLifetimeSafe)
{
    SagaEditor::ViewportContentBuildCoordinator coordinator;
    ASSERT_TRUE(coordinator.Request(Request("active")));
    const auto started = std::chrono::steady_clock::now();
    coordinator.Cancel();
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(50));
}
