/// @file CollaborationModelTests.cpp
/// @brief Unit coverage for the local/offline Block H collaboration model.

#include <SagaCollaboration/CollaborationModel.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

SagaShared::Workspace::WorkspaceDescriptor MakeWorkspace()
{
    SagaShared::Workspace::WorkspaceDescriptor workspace;
    workspace.workspaceId = "workspace-block-h";
    workspace.projectId = "project-block-h";
    workspace.localRoot = "/tmp/saga-block-h";
    workspace.activeUserId = "actor-a";
    return workspace;
}

SagaCollaboration::CollaborationActor MakeActor(const char* id)
{
    SagaCollaboration::CollaborationActor actor;
    actor.actorId = id;
    actor.displayName = id;
    actor.rolePlaceholder = "Reviewer";
    actor.createdByTool = "CollaborationModelTests";
    return actor;
}

SagaCollaboration::SemanticTransaction MakeTransaction(std::uint64_t sequence,
                                                       const char* operationId,
                                                       const char* actorId)
{
    SagaCollaboration::SemanticTransaction transaction;
    transaction.sequence = sequence;
    transaction.operationId = operationId;
    transaction.actorId = actorId;
    transaction.targetArtifactKind =
        SagaCollaboration::CollaborationArtifactKind::PatchPreviewReview;
    transaction.targetPath = "Build/SagaScript/Patches/Quest.patch.json";
    transaction.operationKind = SagaCollaboration::CollaborationOperationKind::ReviewPatchPreview;
    transaction.payload = "{\"review\":\"approved\"}";
    transaction.baseSourceHash = "hash-a";
    return transaction;
}

SagaCollaboration::CollaborationTarget MakeSourceSpanTarget()
{
    SagaCollaboration::CollaborationTarget target;
    target.kind = SagaCollaboration::CollaborationTargetKind::SourceSpan;
    target.sourcePath = "Scripts/Quest.cs";
    target.startByte = 11;
    target.endByte = 22;
    return target;
}

SagaCollaboration::CollaborationTarget MakePatchReportTarget()
{
    SagaCollaboration::CollaborationTarget target;
    target.kind = SagaCollaboration::CollaborationTargetKind::PatchReport;
    target.reportPath = "Build/SagaScript/patch_diff_report.json";
    return target;
}

void RegisterDefaultActorAndArtifact(SagaCollaboration::LocalCollaborationMetadataStore& store)
{
    ASSERT_TRUE(store.RegisterActor(MakeActor("actor-a")));

    SagaCollaboration::CollaborationArtifactRecord artifact;
    artifact.kind = SagaCollaboration::CollaborationArtifactKind::PatchPreviewReview;
    artifact.path = "Build/SagaScript/Patches/Quest.patch.json";
    artifact.sourceHash = "hash-a";
    artifact.reviewStatus = "Pending";
    store.RegisterArtifact(artifact);
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

} // namespace

TEST(CollaborationModelTests, LocalWorkspaceIdentityRejectsMissingActorOrWorkspaceIds)
{
    auto workspace = MakeWorkspace();
    const auto actor = MakeActor("actor-a");

    auto missingWorkspace = SagaCollaboration::MakeWorkspaceIdentity(
        workspace, actor, "CollaborationModelTests");
    missingWorkspace.workspaceId.clear();

    auto missingActor = SagaCollaboration::MakeWorkspaceIdentity(
        workspace, actor, "CollaborationModelTests");
    missingActor.actorId.clear();

    EXPECT_FALSE(SagaCollaboration::IsValid(missingWorkspace));
    EXPECT_FALSE(SagaCollaboration::IsValid(missingActor));
}

TEST(CollaborationModelTests, IdentityMetadataContainsNoSecretsOrOsDerivedPrivateIdentity)
{
    const auto identity = SagaCollaboration::MakeWorkspaceIdentity(
        MakeWorkspace(), MakeActor("actor-a"), "CollaborationModelTests");

    const std::string json = SagaCollaboration::SerializeWorkspaceIdentityJson(identity);

    EXPECT_NE(json.find("\"workspaceId\""), std::string::npos);
    EXPECT_NE(json.find("\"actorId\""), std::string::npos);
    EXPECT_EQ(json.find("email"), std::string::npos);
    EXPECT_EQ(json.find("username"), std::string::npos);
    EXPECT_EQ(json.find("machine"), std::string::npos);
    EXPECT_EQ(json.find("credential"), std::string::npos);
    EXPECT_EQ(json.find("fingerprint"), std::string::npos);
}

TEST(CollaborationModelTests, LocalSessionIdsAreDeterministic)
{
    const auto workspace = MakeWorkspace();
    const auto actor = MakeActor("actor-a");

    const auto first = SagaCollaboration::MakeLocalWorkspaceSession(workspace, actor);
    const auto second = SagaCollaboration::MakeLocalWorkspaceSession(workspace, actor);

    EXPECT_EQ(first.sessionId, "local-workspace:workspace-block-h:actor-a");
    EXPECT_EQ(first.sessionId, second.sessionId);
    EXPECT_TRUE(first.readOnlyMetadataMode);
}

TEST(CollaborationModelTests, DurableWorkspaceStateRoundTripsWithoutPrivateIdentity)
{
    const auto workspace = MakeWorkspace();
    const auto actor = MakeActor("actor-a");

    SagaCollaboration::CollaborationWorkspaceState state;
    state.workspaceId = workspace.workspaceId;
    state.projectId = workspace.projectId;
    state.actors.push_back(actor);
    state.sessions.push_back(SagaCollaboration::MakeLocalWorkspaceSession(workspace, actor));

    SagaCollaboration::ArtifactPresence memory;
    memory.actorId = "actor-a";
    memory.openedProjectId = workspace.projectId;
    memory.activeArtifactKind =
        SagaCollaboration::CollaborationArtifactKind::SagaScriptArtifactReview;
    memory.activeArtifactPath = "Build/SagaScript/Quest.behavior.json";
    memory.activeBehaviorId = "QuestBehavior";
    memory.viewId = "behavior-inspector";
    memory.sequence = 1;
    state.activeArtifactMemory.push_back(memory);

    const std::string json = SagaCollaboration::SerializeWorkspaceStateJson(state);
    std::string diagnostic;
    const auto parsed = SagaCollaboration::ParseWorkspaceStateJson(json, &diagnostic);

    ASSERT_TRUE(parsed.has_value()) << diagnostic;
    EXPECT_TRUE(SagaCollaboration::IsValid(*parsed));
    EXPECT_EQ(parsed->workspaceId, "workspace-block-h");
    EXPECT_NE(json.find("\"tool\": \"SagaCollaboration\""), std::string::npos);
    EXPECT_NE(json.find("\"command\": \"workspace-state\""), std::string::npos);
    EXPECT_EQ(json.find("email"), std::string::npos);
    EXPECT_EQ(json.find("username"), std::string::npos);
    EXPECT_EQ(json.find("machine"), std::string::npos);
    EXPECT_EQ(json.find("credential"), std::string::npos);
}

TEST(CollaborationModelTests, InvalidDurableWorkspaceStateReportsDeterministicDiagnostic)
{
    std::string diagnostic;
    const auto parsed = SagaCollaboration::ParseWorkspaceStateJson(
        R"({"schemaVersion":1,"tool":"SagaCollaboration","command":"workspace-state","projectId":"project"})",
        &diagnostic);

    EXPECT_FALSE(parsed.has_value());
    EXPECT_EQ(diagnostic, "invalid collaboration workspace state");
}

TEST(CollaborationModelTests, PresenceUpdatesDoNotMutateProjectFiles)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "SagaCollaborationModelPresenceTest";
    const std::filesystem::path project = root / "Game.sagaproj";
    std::filesystem::create_directories(root);
    {
        std::ofstream out(project, std::ios::binary | std::ios::trunc);
        out << "stable-project-manifest";
    }

    SagaCollaboration::ArtifactPresence presence;
    presence.actorId = "actor-a";
    presence.openedProjectId = "project-block-h";
    presence.activeArtifactKind =
        SagaCollaboration::CollaborationArtifactKind::SagaScriptArtifactReview;
    presence.activeArtifactPath = "Build/SagaScript/Quest.behavior.json";
    presence.activeBehaviorId = "QuestBehavior";
    presence.viewId = "behavior-inspector";
    presence.readOnlyMode = true;
    presence.sequence = 7;

    const auto report = root / "Build" / "Collaboration" / "presence_report.json";
    ASSERT_TRUE(SagaCollaboration::WritePresenceReport(report, {presence}));

    EXPECT_EQ(ReadFile(project), "stable-project-manifest");
    EXPECT_NE(ReadFile(report).find("\"ephemeral\": true"), std::string::npos);
}

TEST(CollaborationModelTests, ArtifactLocksRejectSecondOwnerAndReleaseDeterministically)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    const std::string resourceId = SagaCollaboration::MakeArtifactResourceId(
        SagaCollaboration::CollaborationArtifactKind::PatchPreviewReview,
        "Build/SagaScript/Patches/Quest.patch.json");

    SagaShared::Collaboration::ResourceLock first;
    first.lockId = "lock-a";
    first.resourceId = resourceId;
    first.participant.value = "actor-a";

    SagaShared::Collaboration::ResourceLock second = first;
    second.lockId = "lock-b";
    second.participant.value = "actor-b";

    EXPECT_TRUE(store.TryAcquireLock(first));
    EXPECT_FALSE(store.TryAcquireLock(second));
    ASSERT_EQ(store.Locks().size(), 1U);

    store.ReleaseLock("lock-a");
    EXPECT_TRUE(store.Locks().empty());
    EXPECT_TRUE(store.TryAcquireLock(second));
    ASSERT_EQ(store.Locks().size(), 1U);
    EXPECT_EQ(store.Locks().front().lockId, "lock-b");
}

TEST(CollaborationModelTests, PersistentLocksClassifyStaleAndSerializeTargetScope)
{
    SagaShared::Collaboration::ResourceLock lock;
    lock.lockId = "lock-source-span";
    lock.participant.value = "actor-a";
    lock.reason = "Reviewing source span";
    lock.expiresAtUnixMs = 100;

    auto record = SagaCollaboration::MakePersistentLock(lock, MakeSourceSpanTarget(), 1);

    EXPECT_EQ(record.lock.resourceId, "source:Scripts/Quest.cs:11-22");
    EXPECT_EQ(SagaCollaboration::ClassifyPersistentLock(record, 99),
              SagaCollaboration::PersistentLockStatus::Active);
    EXPECT_EQ(SagaCollaboration::ClassifyPersistentLock(record, 100),
              SagaCollaboration::PersistentLockStatus::Stale);

    record.status = SagaCollaboration::PersistentLockStatus::Released;
    EXPECT_EQ(SagaCollaboration::ClassifyPersistentLock(record, 1000),
              SagaCollaboration::PersistentLockStatus::Released);

    const std::string report = SagaCollaboration::SerializePersistentLocksReportJson({record});
    EXPECT_NE(report.find("\"command\": \"persistent-locks\""), std::string::npos);
    EXPECT_NE(report.find("\"resourceId\": \"source:Scripts/Quest.cs:11-22\""),
              std::string::npos);
}

TEST(CollaborationModelTests, TransactionLogAppendsInDeterministicSequenceOrder)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);

    auto first = MakeTransaction(1, "op-001", "actor-a");
    auto second = MakeTransaction(2, "op-002", "actor-a");
    second.operationKind = SagaCollaboration::CollaborationOperationKind::MarkArtifactReviewed;

    EXPECT_TRUE(store.SubmitTransaction(first).accepted);
    EXPECT_TRUE(store.SubmitTransaction(second).accepted);

    ASSERT_EQ(store.Transactions().size(), 2U);
    EXPECT_EQ(store.Transactions()[0].sequence, 1U);
    EXPECT_EQ(store.Transactions()[1].sequence, 2U);
    EXPECT_NE(SagaCollaboration::SerializeTransactionJsonLine(store.Transactions()[0])
                  .find("\"operationId\":\"op-001\""),
              std::string::npos);
}

TEST(CollaborationModelTests, CursorPresenceMovementIsNotRecordedAsSemanticTransaction)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;

    SagaCollaboration::ArtifactPresence presence;
    presence.actorId = "actor-a";
    presence.activeArtifactPath = "Build/SagaScript/Quest.behavior.json";
    presence.sequence = 1;

    EXPECT_FALSE(SagaCollaboration::IsSemanticTransactionOperation(
        SagaCollaboration::CollaborationOperationKind::CursorPresenceMoved));
    EXPECT_FALSE(store.RecordPresenceGestureAsTransaction(presence));
    EXPECT_TRUE(store.Transactions().empty());
    ASSERT_EQ(store.Presence().size(), 1U);
}

TEST(CollaborationModelTests, SupportedReviewOperationsAreMetadataOnly)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "SagaCollaborationModelMetadataTest";
    const std::filesystem::path source = root / "Scripts" / "Quest.cs";
    const std::filesystem::path log =
        root / ".saga" / "collaboration" / "workspace_transactions.jsonl";
    std::filesystem::create_directories(source.parent_path());
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        out << "public sealed class Quest {}";
    }

    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);
    ASSERT_TRUE(store.SubmitTransaction(MakeTransaction(1, "op-001", "actor-a")).accepted);
    ASSERT_TRUE(SagaCollaboration::WriteTransactionLog(log, store.Transactions()));

    EXPECT_EQ(ReadFile(source), "public sealed class Quest {}");
    EXPECT_NE(ReadFile(log).find("\"operationKind\":\"ReviewPatchPreview\""),
              std::string::npos);
}

TEST(CollaborationModelTests, ArtifactCommentsAttachResolveAndReopenAsMetadataOnly)
{
    auto artifactTarget = SagaCollaboration::CollaborationTarget{};
    artifactTarget.kind = SagaCollaboration::CollaborationTargetKind::ArtifactPath;
    artifactTarget.artifactPath = "Build/SagaScript/projection_report.json";

    auto behaviorTarget = SagaCollaboration::CollaborationTarget{};
    behaviorTarget.kind = SagaCollaboration::CollaborationTargetKind::BehaviorId;
    behaviorTarget.behaviorId = "QuestBehavior";

    auto nodeTarget = SagaCollaboration::CollaborationTarget{};
    nodeTarget.kind = SagaCollaboration::CollaborationTargetKind::NodeId;
    nodeTarget.nodeId = "quest.open_door";

    auto diagnosticTarget = SagaCollaboration::CollaborationTarget{};
    diagnosticTarget.kind = SagaCollaboration::CollaborationTargetKind::DiagnosticId;
    diagnosticTarget.diagnosticId = "SagaScript.UnsupportedSyntax";

    auto comment = SagaCollaboration::MakeArtifactComment(
        "comment-001",
        "thread-quest",
        "actor-a",
        MakeSourceSpanTarget(),
        "Please check this span.",
        1);
    SagaCollaboration::ResolveComment(comment);
    EXPECT_EQ(comment.status, SagaCollaboration::ArtifactCommentStatus::Resolved);
    SagaCollaboration::ReopenComment(comment);
    EXPECT_EQ(comment.status, SagaCollaboration::ArtifactCommentStatus::Reopened);

    const std::string report = SagaCollaboration::SerializeCommentsReportJson({
        comment,
        SagaCollaboration::MakeArtifactComment("comment-002", "thread-quest", "actor-a", artifactTarget, "artifact", 2),
        SagaCollaboration::MakeArtifactComment("comment-003", "thread-quest", "actor-a", behaviorTarget, "behavior", 3),
        SagaCollaboration::MakeArtifactComment("comment-004", "thread-quest", "actor-a", nodeTarget, "node", 4),
        SagaCollaboration::MakeArtifactComment("comment-005", "thread-quest", "actor-a", MakePatchReportTarget(), "patch", 5),
        SagaCollaboration::MakeArtifactComment("comment-006", "thread-quest", "actor-a", diagnosticTarget, "diagnostic", 6),
    });

    EXPECT_NE(report.find("\"command\": \"artifact-comments\""), std::string::npos);
    EXPECT_NE(report.find("\"status\": \"Reopened\""), std::string::npos);
    EXPECT_NE(report.find("\"resourceId\": \"source:Scripts/Quest.cs:11-22\""),
              std::string::npos);
    EXPECT_NE(report.find("\"resourceId\": \"diagnostic:SagaScript.UnsupportedSyntax\""),
              std::string::npos);
}

TEST(CollaborationModelTests, ReviewRequestsTransitionWithoutApplyingPatch)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "SagaCollaborationModelReviewTest";
    const std::filesystem::path source = root / "Scripts" / "Quest.cs";
    std::filesystem::create_directories(source.parent_path());
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        out << "public sealed class Quest {}";
    }

    auto review = SagaCollaboration::MakeReviewRequest(
        "review-001",
        "actor-a",
        {"actor-b"},
        MakePatchReportTarget(),
        1);
    review.patchDiffReportPath = "Build/SagaScript/patch_diff_report.json";
    review.patchReviewReportPath = "Build/SagaScript/patch_review_report.json";
    review.patchApplyReportPath = "Build/SagaScript/patch_apply_report.json";
    review.expectedSourceHash = "hash-a";
    review.actualSourceHash = "hash-a";

    SagaCollaboration::TransitionReviewRequest(
        review,
        SagaCollaboration::ReviewRequestStatus::InReview);
    EXPECT_EQ(review.status, SagaCollaboration::ReviewRequestStatus::InReview);

    SagaCollaboration::TransitionReviewRequest(
        review,
        SagaCollaboration::ReviewRequestStatus::ApprovedMetadataOnly);
    EXPECT_TRUE(SagaCollaboration::IsReviewApprovalMetadataOnly(review.status));
    EXPECT_EQ(ReadFile(source), "public sealed class Quest {}");

    review.actualSourceHash = "hash-stale";
    SagaCollaboration::RefreshReviewFreshness(review);
    EXPECT_EQ(review.status, SagaCollaboration::ReviewRequestStatus::Stale);

    const std::string report = SagaCollaboration::SerializeReviewReportJson({review});
    EXPECT_NE(report.find("\"command\": \"review-requests\""), std::string::npos);
    EXPECT_NE(report.find("\"metadataOnlyApproval\": false"), std::string::npos);
    EXPECT_EQ(report.find("\"sourceMutation\""), std::string::npos);
}

TEST(CollaborationModelTests, RejectsUnknownActorLockedStaleMissingUnsupportedAndOutOfOrder)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);

    auto unknownActor = MakeTransaction(1, "op-unknown-actor", "actor-missing");
    EXPECT_FALSE(store.SubmitTransaction(unknownActor).accepted);

    auto outOfOrder = MakeTransaction(2, "op-out-of-order", "actor-a");
    EXPECT_FALSE(store.SubmitTransaction(outOfOrder).accepted);

    auto unsupported = MakeTransaction(1, "op-unsupported", "actor-a");
    unsupported.operationKind = SagaCollaboration::CollaborationOperationKind::Unsupported;
    EXPECT_FALSE(store.SubmitTransaction(unsupported).accepted);

    auto missing = MakeTransaction(1, "op-missing", "actor-a");
    missing.targetPath = "Build/SagaScript/Patches/Missing.patch.json";
    EXPECT_FALSE(store.SubmitTransaction(missing).accepted);

    auto stale = MakeTransaction(1, "op-stale", "actor-a");
    stale.baseSourceHash = "hash-old";
    EXPECT_FALSE(store.SubmitTransaction(stale).accepted);

    SagaShared::Collaboration::ResourceLock lock;
    lock.lockId = "lock-b";
    lock.resourceId = SagaCollaboration::MakeArtifactResourceId(
        SagaCollaboration::CollaborationArtifactKind::PatchPreviewReview,
        "Build/SagaScript/Patches/Quest.patch.json");
    lock.participant.value = "actor-b";
    ASSERT_TRUE(store.TryAcquireLock(lock));

    auto locked = MakeTransaction(1, "op-locked", "actor-a");
    EXPECT_FALSE(store.SubmitTransaction(locked).accepted);

    ASSERT_EQ(store.Transactions().size(), 0U);
    ASSERT_EQ(store.Conflicts().size(), 6U);
    EXPECT_EQ(store.Conflicts()[0].category,
              SagaCollaboration::CollaborationConflictCategory::UnknownActor);
    EXPECT_EQ(store.Conflicts()[1].category,
              SagaCollaboration::CollaborationConflictCategory::OutOfOrderOperation);
    EXPECT_EQ(store.Conflicts()[2].category,
              SagaCollaboration::CollaborationConflictCategory::UnsupportedOperation);
    EXPECT_EQ(store.Conflicts()[3].category,
              SagaCollaboration::CollaborationConflictCategory::MissingTargetArtifact);
    EXPECT_EQ(store.Conflicts()[4].category,
              SagaCollaboration::CollaborationConflictCategory::StaleSourceHash);
    EXPECT_EQ(store.Conflicts()[5].category,
              SagaCollaboration::CollaborationConflictCategory::LockedArtifact);
}

TEST(CollaborationModelTests, ConflictReportIsDeterministic)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);

    auto stale = MakeTransaction(1, "op-stale", "actor-a");
    stale.baseSourceHash = "hash-old";
    EXPECT_FALSE(store.SubmitTransaction(stale).accepted);

    const std::string report =
        SagaCollaboration::SerializeConflictReportJson(store.Conflicts());

    EXPECT_NE(report.find("\"conflictId\": \"conflict:op-stale:StaleSourceHash\""),
              std::string::npos);
    EXPECT_NE(report.find("\"category\": \"StaleSourceHash\""), std::string::npos);
    EXPECT_EQ(report.find("timestamp"), std::string::npos);
}

TEST(CollaborationModelTests, DangerousOperationChecksAreLocalMetadataDiagnosticsOnly)
{
    const std::vector<SagaCollaboration::RoleAssignment> roles{
        {"actor-owner", SagaCollaboration::TeamRole::Owner},
        {"actor-programmer", SagaCollaboration::TeamRole::Programmer},
        {"actor-designer", SagaCollaboration::TeamRole::Designer},
        {"actor-qa", SagaCollaboration::TeamRole::QA},
    };

    const auto owner = SagaCollaboration::CheckDangerousOperation(
        "role-check-001",
        "actor-owner",
        roles,
        SagaCollaboration::DangerousOperationKind::DeleteScene,
        "scene:intro");
    const auto designer = SagaCollaboration::CheckDangerousOperation(
        "role-check-002",
        "actor-designer",
        roles,
        SagaCollaboration::DangerousOperationKind::ChangePackageProfile,
        "package:technical-preview-server-headless");
    const auto qa = SagaCollaboration::CheckDangerousOperation(
        "role-check-003",
        "actor-qa",
        roles,
        SagaCollaboration::DangerousOperationKind::ApprovePublishGate,
        "publish:local");
    const auto missing = SagaCollaboration::CheckDangerousOperation(
        "role-check-004",
        "actor-missing",
        roles,
        SagaCollaboration::DangerousOperationKind::OverrideLock,
        "lock:quest");

    EXPECT_EQ(owner.status,
              SagaCollaboration::DangerousOperationCheckStatus::AllowedByLocalMetadata);
    EXPECT_EQ(designer.status,
              SagaCollaboration::DangerousOperationCheckStatus::BlockedByLocalMetadata);
    EXPECT_EQ(qa.status,
              SagaCollaboration::DangerousOperationCheckStatus::AllowedByLocalMetadata);
    EXPECT_EQ(missing.status,
              SagaCollaboration::DangerousOperationCheckStatus::UnknownActor);

    const std::string report =
        SagaCollaboration::SerializeRoleCheckReportJson({owner, designer, qa, missing});
    EXPECT_NE(report.find("\"command\": \"role-check\""), std::string::npos);
    EXPECT_NE(report.find("\"status\": \"BlockedByLocalMetadata\""), std::string::npos);
    EXPECT_EQ(report.find("credential"), std::string::npos);
}

TEST(CollaborationModelTests, TeamRoomDashboardSummarizesLocalStateWithoutQt)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);

    ASSERT_TRUE(store.RegisterActor(MakeActor("actor-b")));

    SagaCollaboration::CollaborationArtifactRecord diagnostic;
    diagnostic.kind = SagaCollaboration::CollaborationArtifactKind::DiagnosticsReview;
    diagnostic.path = "Build/Diagnostics/latest.json";
    diagnostic.sourceHash = "diag-hash";
    diagnostic.reviewStatus = "Acknowledged";
    store.RegisterArtifact(diagnostic);

    SagaCollaboration::ArtifactPresence presence;
    presence.actorId = "actor-a";
    presence.openedProjectId = "project-block-h";
    presence.activeArtifactKind = SagaCollaboration::CollaborationArtifactKind::DiagnosticsReview;
    presence.activeArtifactPath = "Build/Diagnostics/latest.json";
    presence.viewId = "team-room";
    presence.sequence = 3;
    store.UpdatePresence(presence);

    store.RegisterComment(SagaCollaboration::MakeArtifactComment(
        "comment-001",
        "thread-quest",
        "actor-a",
        MakeSourceSpanTarget(),
        "Open review note",
        1));
    auto review = SagaCollaboration::MakeReviewRequest(
        "review-001",
        "actor-a",
        {"actor-b"},
        MakePatchReportTarget(),
        1);
    SagaCollaboration::TransitionReviewRequest(
        review,
        SagaCollaboration::ReviewRequestStatus::Requested);
    store.RegisterReviewRequest(review);
    store.RegisterDangerousOperationCheck(SagaCollaboration::CheckDangerousOperation(
        "role-check-001",
        "actor-a",
        {{"actor-a", SagaCollaboration::TeamRole::Owner}},
        SagaCollaboration::DangerousOperationKind::OverrideLock,
        "lock:quest"));

    ASSERT_TRUE(store.SubmitTransaction(MakeTransaction(1, "op-001", "actor-a")).accepted);

    const auto dashboard = store.BuildDashboardView();
    const std::string json = SagaCollaboration::SerializeTeamRoomDashboardJson(dashboard);

    EXPECT_EQ(dashboard.actors.size(), 2U);
    EXPECT_EQ(dashboard.presence.size(), 1U);
    EXPECT_EQ(dashboard.recentTransactions.size(), 1U);
    EXPECT_EQ(dashboard.diagnosticsReviewStatus.size(), 1U);
    EXPECT_EQ(dashboard.comments.size(), 1U);
    EXPECT_EQ(dashboard.reviewQueue.size(), 1U);
    EXPECT_EQ(dashboard.dangerousOperationChecks.size(), 1U);
    EXPECT_EQ(dashboard.packageStatusPlaceholder, "Deferred");
    EXPECT_EQ(dashboard.publishStatusPlaceholder, "Deferred");
    EXPECT_NE(json.find("\"command\": \"team-room-status\""), std::string::npos);
    EXPECT_NE(json.find("\"reviewQueue\""), std::string::npos);
    EXPECT_NE(json.find("\"comments\""), std::string::npos);
    EXPECT_NE(json.find("\"dangerousOperationChecks\""), std::string::npos);
    EXPECT_NE(json.find("\"publishStatusPlaceholder\": \"Deferred\""), std::string::npos);
    EXPECT_EQ(json.find("Qt"), std::string::npos);
    EXPECT_EQ(json.find("realtime"), std::string::npos);
    EXPECT_EQ(json.find("cloud"), std::string::npos);
}

TEST(CollaborationModelTests, SamePatchPreviewReviewByDifferentActorsIsRejected)
{
    SagaCollaboration::LocalCollaborationMetadataStore store;
    RegisterDefaultActorAndArtifact(store);
    ASSERT_TRUE(store.RegisterActor(MakeActor("actor-b")));

    ASSERT_TRUE(store.SubmitTransaction(MakeTransaction(1, "op-001", "actor-a")).accepted);
    auto second = MakeTransaction(2, "op-002", "actor-b");

    const auto result = store.SubmitTransaction(second);

    EXPECT_FALSE(result.accepted);
    ASSERT_TRUE(result.conflict.has_value());
    EXPECT_EQ(result.conflict->category,
              SagaCollaboration::CollaborationConflictCategory::
                  SamePatchPreviewReviewedByMultipleActors);
    ASSERT_EQ(store.Transactions().size(), 1U);
}
