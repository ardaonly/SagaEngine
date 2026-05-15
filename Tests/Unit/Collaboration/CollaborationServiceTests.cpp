/// @file CollaborationServiceTests.cpp
/// @brief Unit coverage for SagaCollaboration service boundary skeletons.

#include <SagaCollaboration/CollaborationService.hpp>
#include <SagaShared/Collaboration/ParticipantId.hpp>
#include <SagaShared/Workspace/WorkspaceDescriptor.hpp>

#include <gtest/gtest.h>

namespace
{

SagaShared::Workspace::WorkspaceDescriptor MakeWorkspace()
{
    SagaShared::Workspace::WorkspaceDescriptor workspace;
    workspace.workspaceId = "workspace-basic";
    workspace.projectId = "project-basic";
    workspace.localRoot = "/tmp/saga-basic";
    workspace.activeUserId = "user-host";
    return workspace;
}

SagaShared::Collaboration::ParticipantId MakeParticipant(const char* value)
{
    SagaShared::Collaboration::ParticipantId participant;
    participant.value = value;
    return participant;
}

} // namespace

TEST(CollaborationServiceTests, StartsLocalSessionWithWorkspaceContext)
{
    SagaCollaboration::CollaborationService service;
    const auto host = MakeParticipant("user-host");

    const auto session = service.Sessions().StartLocalSession(MakeWorkspace(), host);

    ASSERT_TRUE(service.Sessions().CurrentSession().has_value());
    EXPECT_EQ(session.projectId, "project-basic");
    EXPECT_EQ(session.host.value, "user-host");
    ASSERT_EQ(session.participants.size(), 1U);
    EXPECT_EQ(session.participants.front().value, "user-host");
    EXPECT_EQ(session.workspace.workspaceId, "workspace-basic");
}

TEST(CollaborationServiceTests, UpdatesPresenceByParticipant)
{
    SagaCollaboration::CollaborationService service;

    SagaShared::Collaboration::PresenceSnapshot first;
    first.participant = MakeParticipant("user-a");
    first.openResource = "scene-a";
    service.Presence().UpdatePresence(first);

    SagaShared::Collaboration::PresenceSnapshot second = first;
    second.openResource = "scene-b";
    second.revision = 2;
    service.Presence().UpdatePresence(second);

    const auto presence = service.Presence().ListPresence();
    ASSERT_EQ(presence.size(), 1U);
    EXPECT_EQ(presence.front().openResource, "scene-b");
    EXPECT_EQ(presence.front().revision, 2U);
}

TEST(CollaborationServiceTests, EnforcesSimplePermissionGrants)
{
    SagaCollaboration::CollaborationService service;
    const auto participant = MakeParticipant("user-a");

    SagaShared::Collaboration::PermissionGrant grant;
    grant.participant = participant;
    grant.permission = "scene.edit";
    grant.resourceId = "scene-a";
    service.Permissions().Grant(grant);

    EXPECT_TRUE(service.Permissions().CanPerform(participant, "scene.edit", "scene-a"));
    EXPECT_FALSE(service.Permissions().CanPerform(participant, "scene.edit", "scene-b"));
    EXPECT_FALSE(service.Permissions().CanPerform(participant, "publish", "scene-a"));
}

TEST(CollaborationServiceTests, RejectsConflictingResourceLocks)
{
    SagaCollaboration::CollaborationService service;

    SagaShared::Collaboration::ResourceLock first;
    first.lockId = "lock-a";
    first.resourceId = "scene-a";
    first.participant = MakeParticipant("user-a");

    SagaShared::Collaboration::ResourceLock second = first;
    second.lockId = "lock-b";
    second.participant = MakeParticipant("user-b");

    EXPECT_TRUE(service.Locks().TryLock(first));
    EXPECT_FALSE(service.Locks().TryLock(second));
    ASSERT_EQ(service.Locks().Locks().size(), 1U);
    EXPECT_EQ(service.Locks().Locks().front().lockId, "lock-a");
}

TEST(CollaborationServiceTests, AssignsMonotonicChangeRevisions)
{
    SagaCollaboration::CollaborationService service;

    SagaCollaboration::ChangeStreamEntry first;
    first.participant = MakeParticipant("user-a");
    first.resourceId = "scene-a";
    first.operation = "edit";

    const auto storedFirst = service.Changes().Append(first);
    const auto storedSecond = service.Changes().Append(first);

    EXPECT_EQ(storedFirst.revision, 1U);
    EXPECT_EQ(storedSecond.revision, 2U);

    const auto sinceFirst = service.Changes().Since(1);
    ASSERT_EQ(sinceFirst.size(), 1U);
    EXPECT_EQ(sinceFirst.front().revision, 2U);
}
