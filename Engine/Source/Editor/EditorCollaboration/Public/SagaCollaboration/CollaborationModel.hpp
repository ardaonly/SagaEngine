/// @file CollaborationModel.hpp
/// @brief Local/offline collaboration metadata model for Technical Evaluation.

#pragma once

#include "SagaShared/Collaboration/CollaborationDiagnostic.hpp"
#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/ResourceLock.hpp"
#include "SagaShared/Workspace/WorkspaceDescriptor.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaCollaboration
{

enum class CollaborationArtifactKind : std::uint8_t
{
    ProjectMetadata,
    SagaScriptArtifactReview,
    PatchEvaluationReview,
    DiagnosticsReview,
    Notes,
    Unsupported,
};

enum class CollaborationOperationKind : std::uint8_t
{
    ReviewPatchEvaluation,
    MarkArtifactReviewed,
    AddNote,
    ResolveNote,
    AcknowledgeDiagnostic,
    CursorPresenceMoved,
    Unsupported,
};

enum class CollaborationTransactionStatus : std::uint8_t
{
    Accepted,
    Rejected,
};

enum class CollaborationConflictCategory : std::uint8_t
{
    SamePatchEvaluationReviewedByMultipleActors,
    StaleSourceHash,
    MissingTargetArtifact,
    UnsupportedOperation,
    OutOfOrderOperation,
    UnknownActor,
    LockedArtifact,
};

enum class CollaborationTargetKind : std::uint8_t
{
    ArtifactPath,
    BehaviorId,
    NodeId,
    SourceSpan,
    PatchReport,
    DiagnosticId,
    Unsupported,
};

enum class PersistentLockStatus : std::uint8_t
{
    Active,
    Released,
    Stale,
    Conflict,
};

enum class ArtifactCommentStatus : std::uint8_t
{
    Open,
    Resolved,
    Reopened,
};

enum class ReviewRequestStatus : std::uint8_t
{
    Requested,
    InReview,
    ApprovedMetadataOnly,
    Rejected,
    Blocked,
    Stale,
    Superseded,
};

enum class TeamRole : std::uint8_t
{
    Owner,
    Programmer,
    Designer,
    Artist,
    QA,
    Unknown,
};

enum class DangerousOperationKind : std::uint8_t
{
    DeleteScene,
    DeleteBehaviorSource,
    ChangePackageProfile,
    OverrideLock,
    ApprovePublishGate,
    ModifyRuntimeBindingMetadataManually,
    Unknown,
};

enum class DangerousOperationCheckStatus : std::uint8_t
{
    AllowedByLocalMetadata,
    BlockedByLocalMetadata,
    UnknownActor,
    UnknownOperation,
};

struct CollaborationActor
{
    std::string actorId;
    std::string displayName;
    std::string rolePlaceholder;
    std::string createdByTool;
};

struct WorkspaceIdentity
{
    std::uint32_t schemaVersion = 0;
    std::string workspaceId;
    std::string actorId;
    std::string displayName;
    std::string rolePlaceholder;
    std::string createdByTool;
};

struct LocalWorkspaceSession
{
    std::string sessionId;
    std::string workspaceId;
    std::string projectId;
    CollaborationActor actor;
    bool readOnlyMetadataMode = true;
};

struct ArtifactPresence
{
    std::string actorId;
    std::string openedProjectId;
    CollaborationArtifactKind activeArtifactKind = CollaborationArtifactKind::Unsupported;
    std::string activeArtifactPath;
    std::string activeBehaviorId;
    std::string viewId;
    bool readOnlyMode = true;
    std::uint64_t sequence = 0;
};

struct CollaborationArtifactRecord
{
    CollaborationArtifactKind kind = CollaborationArtifactKind::Unsupported;
    std::string path;
    std::string sourceHash;
    std::string reviewStatus;
};

struct SemanticTransaction
{
    std::uint64_t sequence = 0;
    std::string operationId;
    std::string actorId;
    CollaborationArtifactKind targetArtifactKind = CollaborationArtifactKind::Unsupported;
    std::string targetPath;
    CollaborationOperationKind operationKind = CollaborationOperationKind::Unsupported;
    std::string payload;
    CollaborationTransactionStatus status = CollaborationTransactionStatus::Accepted;
    std::optional<std::string> baseSourceHash;
};

struct CollaborationConflict
{
    std::string conflictId;
    CollaborationConflictCategory category = CollaborationConflictCategory::UnsupportedOperation;
    std::string operationId;
    std::string actorId;
    CollaborationArtifactKind targetArtifactKind = CollaborationArtifactKind::Unsupported;
    std::string targetPath;
    std::string reason;
};

struct TransactionSubmission
{
    SemanticTransaction transaction;
    bool accepted = false;
    std::optional<CollaborationConflict> conflict;
};

struct CollaborationTarget
{
    CollaborationTargetKind kind = CollaborationTargetKind::Unsupported;
    std::string artifactPath;
    std::string behaviorId;
    std::string nodeId;
    std::string sourcePath;
    std::uint64_t startByte = 0;
    std::uint64_t endByte = 0;
    std::string reportPath;
    std::string diagnosticId;
};

struct PersistentArtifactLock
{
    SagaShared::Collaboration::ResourceLock lock;
    CollaborationTarget target;
    PersistentLockStatus status = PersistentLockStatus::Active;
    std::uint64_t sequence = 0;
};

struct ArtifactComment
{
    std::string commentId;
    std::string threadId;
    std::string authorActorId;
    CollaborationTarget target;
    std::string body;
    std::uint64_t sequence = 0;
    ArtifactCommentStatus status = ArtifactCommentStatus::Open;
};

struct ReviewRequest
{
    std::string reviewId;
    std::string requesterActorId;
    std::vector<std::string> requestedReviewerActorIds;
    CollaborationTarget target;
    std::string patchDiffReportPath;
    std::string patchReviewReportPath;
    std::string patchApplyReportPath;
    std::string expectedSourceHash;
    std::string actualSourceHash;
    std::uint64_t sequence = 0;
    ReviewRequestStatus status = ReviewRequestStatus::Requested;
};

struct RoleAssignment
{
    std::string actorId;
    TeamRole role = TeamRole::Unknown;
};

struct DangerousOperationCheck
{
    std::string operationId;
    std::string actorId;
    TeamRole role = TeamRole::Unknown;
    DangerousOperationKind operation = DangerousOperationKind::Unknown;
    std::string resourceId;
    DangerousOperationCheckStatus status = DangerousOperationCheckStatus::BlockedByLocalMetadata;
    std::string reason;
};

struct CollaborationWorkspaceState
{
    std::uint32_t schemaVersion = 1;
    std::string tool = "SagaCollaboration";
    std::string command = "workspace-state";
    std::string status = "Passed";
    std::string workspaceId;
    std::string projectId;
    std::vector<CollaborationActor> actors;
    std::vector<LocalWorkspaceSession> sessions;
    std::vector<ArtifactPresence> activeArtifactMemory;
    std::vector<SagaShared::Collaboration::CollaborationDiagnostic> diagnostics;
};

struct TeamRoomDashboardView
{
    std::vector<CollaborationActor> actors;
    std::vector<ArtifactPresence> presence;
    std::vector<SagaShared::Collaboration::ResourceLock> locks;
    std::vector<SemanticTransaction> recentTransactions;
    std::vector<CollaborationConflict> conflicts;
    std::vector<CollaborationArtifactRecord> diagnosticsReviewStatus;
    std::vector<ArtifactComment> comments;
    std::vector<ReviewRequest> reviewQueue;
    std::vector<DangerousOperationCheck> dangerousOperationChecks;
    std::string packageStatusPlaceholder;
    std::string publishStatusPlaceholder;
};

[[nodiscard]] const char* ToString(CollaborationArtifactKind kind) noexcept;
[[nodiscard]] const char* ToString(CollaborationOperationKind kind) noexcept;
[[nodiscard]] const char* ToString(CollaborationTransactionStatus status) noexcept;
[[nodiscard]] const char* ToString(CollaborationConflictCategory category) noexcept;
[[nodiscard]] const char* ToString(CollaborationTargetKind kind) noexcept;
[[nodiscard]] const char* ToString(PersistentLockStatus status) noexcept;
[[nodiscard]] const char* ToString(ArtifactCommentStatus status) noexcept;
[[nodiscard]] const char* ToString(ReviewRequestStatus status) noexcept;
[[nodiscard]] const char* ToString(TeamRole role) noexcept;
[[nodiscard]] const char* ToString(DangerousOperationKind operation) noexcept;
[[nodiscard]] const char* ToString(DangerousOperationCheckStatus status) noexcept;

[[nodiscard]] bool IsValid(const CollaborationActor& actor) noexcept;
[[nodiscard]] bool IsValid(const WorkspaceIdentity& identity) noexcept;
[[nodiscard]] bool IsValid(const CollaborationWorkspaceState& state) noexcept;
[[nodiscard]] bool IsLockableArtifactKind(CollaborationArtifactKind kind) noexcept;
[[nodiscard]] bool IsSemanticTransactionOperation(CollaborationOperationKind operation) noexcept;
[[nodiscard]] bool IsReviewApprovalMetadataOnly(ReviewRequestStatus status) noexcept;

[[nodiscard]] std::string MakeArtifactResourceId(CollaborationArtifactKind kind,
                                                 const std::string& path);
[[nodiscard]] std::string MakeTargetResourceId(const CollaborationTarget& target);
[[nodiscard]] std::string MakeLocalWorkspaceSessionId(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const CollaborationActor& actor);
[[nodiscard]] WorkspaceIdentity MakeWorkspaceIdentity(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const CollaborationActor& actor,
    std::string createdByTool);
[[nodiscard]] LocalWorkspaceSession MakeLocalWorkspaceSession(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    CollaborationActor actor);

[[nodiscard]] std::string SerializeWorkspaceIdentityJson(const WorkspaceIdentity& identity);
[[nodiscard]] std::optional<WorkspaceIdentity> ParseWorkspaceIdentityJson(
    const std::string& content,
    std::string* diagnostic = nullptr);
[[nodiscard]] bool WriteWorkspaceIdentity(const std::filesystem::path& path,
                                          const WorkspaceIdentity& identity,
                                          std::string* diagnostic = nullptr);
[[nodiscard]] std::optional<WorkspaceIdentity> ReadWorkspaceIdentity(
    const std::filesystem::path& path,
    std::string* diagnostic = nullptr);

[[nodiscard]] std::string SerializeWorkspaceStateJson(const CollaborationWorkspaceState& state);
[[nodiscard]] std::optional<CollaborationWorkspaceState> ParseWorkspaceStateJson(
    const std::string& content,
    std::string* diagnostic = nullptr);
[[nodiscard]] bool WriteWorkspaceState(const std::filesystem::path& path,
                                       const CollaborationWorkspaceState& state,
                                       std::string* diagnostic = nullptr);
[[nodiscard]] std::optional<CollaborationWorkspaceState> ReadWorkspaceState(
    const std::filesystem::path& path,
    std::string* diagnostic = nullptr);

[[nodiscard]] std::string SerializePresenceReportJson(
    const std::vector<ArtifactPresence>& presence);
[[nodiscard]] bool WritePresenceReport(const std::filesystem::path& path,
                                       const std::vector<ArtifactPresence>& presence,
                                       std::string* diagnostic = nullptr);

[[nodiscard]] PersistentArtifactLock MakePersistentLock(
    SagaShared::Collaboration::ResourceLock lock,
    CollaborationTarget target,
    std::uint64_t sequence);
[[nodiscard]] PersistentLockStatus ClassifyPersistentLock(
    const PersistentArtifactLock& lock,
    std::uint64_t nowUnixMs) noexcept;
[[nodiscard]] std::string SerializePersistentLocksReportJson(
    const std::vector<PersistentArtifactLock>& locks);

[[nodiscard]] ArtifactComment MakeArtifactComment(
    std::string commentId,
    std::string threadId,
    std::string authorActorId,
    CollaborationTarget target,
    std::string body,
    std::uint64_t sequence);
void ResolveComment(ArtifactComment& comment) noexcept;
void ReopenComment(ArtifactComment& comment) noexcept;
[[nodiscard]] std::string SerializeCommentsReportJson(
    const std::vector<ArtifactComment>& comments);

[[nodiscard]] ReviewRequest MakeReviewRequest(
    std::string reviewId,
    std::string requesterActorId,
    std::vector<std::string> requestedReviewerActorIds,
    CollaborationTarget target,
    std::uint64_t sequence);
void TransitionReviewRequest(ReviewRequest& review, ReviewRequestStatus status) noexcept;
void RefreshReviewFreshness(ReviewRequest& review) noexcept;
[[nodiscard]] std::string SerializeReviewReportJson(
    const std::vector<ReviewRequest>& reviews);

[[nodiscard]] DangerousOperationCheck CheckDangerousOperation(
    std::string operationId,
    const std::string& actorId,
    const std::vector<RoleAssignment>& roles,
    DangerousOperationKind operation,
    std::string resourceId);
[[nodiscard]] std::string SerializeRoleCheckReportJson(
    const std::vector<DangerousOperationCheck>& checks);

[[nodiscard]] std::string SerializeTransactionJsonLine(const SemanticTransaction& transaction);
[[nodiscard]] bool WriteTransactionLog(const std::filesystem::path& path,
                                       const std::vector<SemanticTransaction>& transactions,
                                       std::string* diagnostic = nullptr);

[[nodiscard]] std::string SerializeConflictReportJson(
    const std::vector<CollaborationConflict>& conflicts);
[[nodiscard]] bool WriteConflictReport(const std::filesystem::path& path,
                                       const std::vector<CollaborationConflict>& conflicts,
                                       std::string* diagnostic = nullptr);

[[nodiscard]] std::string SerializeTeamRoomDashboardJson(
    const TeamRoomDashboardView& dashboard);

class LocalCollaborationMetadataStore
{
public:
    [[nodiscard]] bool RegisterActor(CollaborationActor actor);
    void RegisterArtifact(CollaborationArtifactRecord artifact);
    void UpdatePresence(ArtifactPresence presence);

    [[nodiscard]] bool TryAcquireLock(SagaShared::Collaboration::ResourceLock lock);
    void ReleaseLock(const std::string& lockId);
    void RegisterComment(ArtifactComment comment);
    void RegisterReviewRequest(ReviewRequest review);
    void RegisterDangerousOperationCheck(DangerousOperationCheck check);

    [[nodiscard]] TransactionSubmission SubmitTransaction(SemanticTransaction transaction);
    [[nodiscard]] bool RecordPresenceGestureAsTransaction(const ArtifactPresence& presence);

    [[nodiscard]] const std::vector<CollaborationActor>& Actors() const noexcept;
    [[nodiscard]] const std::vector<ArtifactPresence>& Presence() const noexcept;
    [[nodiscard]] const std::vector<SagaShared::Collaboration::ResourceLock>& Locks() const noexcept;
    [[nodiscard]] const std::vector<ArtifactComment>& Comments() const noexcept;
    [[nodiscard]] const std::vector<ReviewRequest>& ReviewRequests() const noexcept;
    [[nodiscard]] const std::vector<DangerousOperationCheck>& DangerousOperationChecks() const noexcept;
    [[nodiscard]] const std::vector<SemanticTransaction>& Transactions() const noexcept;
    [[nodiscard]] const std::vector<CollaborationConflict>& Conflicts() const noexcept;

    [[nodiscard]] TeamRoomDashboardView BuildDashboardView() const;

private:
    [[nodiscard]] bool HasActor(const std::string& actorId) const;
    [[nodiscard]] const CollaborationArtifactRecord* FindArtifact(
        CollaborationArtifactKind kind,
        const std::string& path) const;
    [[nodiscard]] const SagaShared::Collaboration::ResourceLock* FindLock(
        const std::string& resourceId) const;
    [[nodiscard]] CollaborationConflict MakeConflict(
        const SemanticTransaction& transaction,
        CollaborationConflictCategory category,
        std::string reason) const;
    [[nodiscard]] TransactionSubmission Reject(SemanticTransaction transaction,
                                               CollaborationConflictCategory category,
                                               std::string reason);

    std::vector<CollaborationActor> m_actors;
    std::vector<CollaborationArtifactRecord> m_artifacts;
    std::vector<ArtifactPresence> m_presence;
    std::vector<SagaShared::Collaboration::ResourceLock> m_locks;
    std::vector<ArtifactComment> m_comments;
    std::vector<ReviewRequest> m_reviews;
    std::vector<DangerousOperationCheck> m_dangerousOperationChecks;
    std::vector<SemanticTransaction> m_transactions;
    std::vector<CollaborationConflict> m_conflicts;
    std::uint64_t m_nextSequence = 1;
};

} // namespace SagaCollaboration
