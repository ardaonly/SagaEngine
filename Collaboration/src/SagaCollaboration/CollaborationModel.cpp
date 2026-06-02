/// @file CollaborationModel.cpp
/// @brief Local/offline collaboration metadata model implementation.

#include "SagaCollaboration/CollaborationModel.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <utility>

namespace SagaCollaboration
{
namespace
{

using Json = nlohmann::ordered_json;

[[nodiscard]] Json ActorToJson(const CollaborationActor& actor)
{
    return Json{
        {"actorId", actor.actorId},
        {"displayName", actor.displayName},
        {"rolePlaceholder", actor.rolePlaceholder},
        {"createdByTool", actor.createdByTool},
    };
}

[[nodiscard]] Json PresenceToJson(const ArtifactPresence& presence)
{
    return Json{
        {"actorId", presence.actorId},
        {"openedProjectId", presence.openedProjectId},
        {"activeArtifactKind", ToString(presence.activeArtifactKind)},
        {"activeArtifactPath", presence.activeArtifactPath},
        {"activeBehaviorId", presence.activeBehaviorId},
        {"viewId", presence.viewId},
        {"readOnlyMode", presence.readOnlyMode},
        {"sequence", presence.sequence},
    };
}

[[nodiscard]] Json LockToJson(const SagaShared::Collaboration::ResourceLock& lock)
{
    return Json{
        {"lockId", lock.lockId},
        {"resourceId", lock.resourceId},
        {"actorId", lock.participant.value},
        {"reason", lock.reason},
        {"revocable", lock.revocable},
        {"expiresAtUnixMs", lock.expiresAtUnixMs},
    };
}

[[nodiscard]] Json TransactionToJson(const SemanticTransaction& transaction)
{
    Json value{
        {"sequence", transaction.sequence},
        {"operationId", transaction.operationId},
        {"actorId", transaction.actorId},
        {"targetArtifactKind", ToString(transaction.targetArtifactKind)},
        {"targetPath", transaction.targetPath},
        {"operationKind", ToString(transaction.operationKind)},
        {"payload", transaction.payload},
        {"status", ToString(transaction.status)},
    };
    if (transaction.baseSourceHash.has_value())
    {
        value["baseSourceHash"] = *transaction.baseSourceHash;
    }
    return value;
}

[[nodiscard]] Json ConflictToJson(const CollaborationConflict& conflict)
{
    return Json{
        {"conflictId", conflict.conflictId},
        {"category", ToString(conflict.category)},
        {"operationId", conflict.operationId},
        {"actorId", conflict.actorId},
        {"targetArtifactKind", ToString(conflict.targetArtifactKind)},
        {"targetPath", conflict.targetPath},
        {"reason", conflict.reason},
    };
}

[[nodiscard]] Json ArtifactToJson(const CollaborationArtifactRecord& artifact)
{
    return Json{
        {"targetArtifactKind", ToString(artifact.kind)},
        {"targetPath", artifact.path},
        {"sourceHash", artifact.sourceHash},
        {"reviewStatus", artifact.reviewStatus},
    };
}

[[nodiscard]] const char* ToString(
    SagaShared::Collaboration::CollaborationDiagnosticSeverity severity) noexcept
{
    using Severity = SagaShared::Collaboration::CollaborationDiagnosticSeverity;
    switch (severity)
    {
    case Severity::Info:
        return "Info";
    case Severity::Warning:
        return "Warning";
    case Severity::Error:
        return "Error";
    }
    return "Info";
}

[[nodiscard]] Json DiagnosticToJson(
    const SagaShared::Collaboration::CollaborationDiagnostic& diagnostic)
{
    return Json{
        {"severity", ToString(diagnostic.severity)},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
        {"resourceId", diagnostic.resourceId},
    };
}

[[nodiscard]] Json SessionToJson(const LocalWorkspaceSession& session)
{
    return Json{
        {"sessionId", session.sessionId},
        {"workspaceId", session.workspaceId},
        {"projectId", session.projectId},
        {"actor", ActorToJson(session.actor)},
        {"readOnlyMetadataMode", session.readOnlyMetadataMode},
    };
}

[[nodiscard]] Json TargetToJson(const CollaborationTarget& target)
{
    return Json{
        {"kind", ToString(target.kind)},
        {"artifactPath", target.artifactPath},
        {"behaviorId", target.behaviorId},
        {"nodeId", target.nodeId},
        {"sourcePath", target.sourcePath},
        {"startByte", target.startByte},
        {"endByte", target.endByte},
        {"reportPath", target.reportPath},
        {"diagnosticId", target.diagnosticId},
        {"resourceId", MakeTargetResourceId(target)},
    };
}

[[nodiscard]] Json PersistentLockToJson(const PersistentArtifactLock& lock)
{
    return Json{
        {"sequence", lock.sequence},
        {"status", ToString(lock.status)},
        {"lock", LockToJson(lock.lock)},
        {"target", TargetToJson(lock.target)},
    };
}

[[nodiscard]] Json CommentToJson(const ArtifactComment& comment)
{
    return Json{
        {"sequence", comment.sequence},
        {"commentId", comment.commentId},
        {"threadId", comment.threadId},
        {"authorActorId", comment.authorActorId},
        {"status", ToString(comment.status)},
        {"target", TargetToJson(comment.target)},
        {"body", comment.body},
    };
}

[[nodiscard]] Json ReviewToJson(const ReviewRequest& review)
{
    Json reviewers = Json::array();
    for (const std::string& actorId : review.requestedReviewerActorIds)
    {
        reviewers.push_back(actorId);
    }

    return Json{
        {"sequence", review.sequence},
        {"reviewId", review.reviewId},
        {"requesterActorId", review.requesterActorId},
        {"requestedReviewerActorIds", reviewers},
        {"status", ToString(review.status)},
        {"target", TargetToJson(review.target)},
        {"patchDiffReportPath", review.patchDiffReportPath},
        {"patchReviewReportPath", review.patchReviewReportPath},
        {"patchApplyReportPath", review.patchApplyReportPath},
        {"expectedSourceHash", review.expectedSourceHash},
        {"actualSourceHash", review.actualSourceHash},
        {"metadataOnlyApproval", IsReviewApprovalMetadataOnly(review.status)},
    };
}

[[nodiscard]] Json DangerousOperationCheckToJson(const DangerousOperationCheck& check)
{
    return Json{
        {"operationId", check.operationId},
        {"actorId", check.actorId},
        {"role", ToString(check.role)},
        {"operation", ToString(check.operation)},
        {"resourceId", check.resourceId},
        {"status", ToString(check.status)},
        {"reason", check.reason},
    };
}

[[nodiscard]] CollaborationActor ActorFromJson(const Json& value)
{
    CollaborationActor actor;
    actor.actorId = value.value("actorId", "");
    actor.displayName = value.value("displayName", "");
    actor.rolePlaceholder = value.value("rolePlaceholder", "");
    actor.createdByTool = value.value("createdByTool", "");
    return actor;
}

[[nodiscard]] ArtifactPresence PresenceFromJson(const Json& value)
{
    ArtifactPresence presence;
    presence.actorId = value.value("actorId", "");
    presence.openedProjectId = value.value("openedProjectId", "");
    presence.activeArtifactPath = value.value("activeArtifactPath", "");
    presence.activeBehaviorId = value.value("activeBehaviorId", "");
    presence.viewId = value.value("viewId", "");
    presence.readOnlyMode = value.value("readOnlyMode", true);
    presence.sequence = value.value("sequence", 0ULL);
    return presence;
}

[[nodiscard]] LocalWorkspaceSession SessionFromJson(const Json& value)
{
    LocalWorkspaceSession session;
    session.sessionId = value.value("sessionId", "");
    session.workspaceId = value.value("workspaceId", "");
    session.projectId = value.value("projectId", "");
    if (value.contains("actor") && value["actor"].is_object())
    {
        session.actor = ActorFromJson(value["actor"]);
    }
    session.readOnlyMetadataMode = value.value("readOnlyMetadataMode", true);
    return session;
}

[[nodiscard]] bool WriteTextFile(const std::filesystem::path& path,
                                 const std::string& content,
                                 std::string* diagnostic)
{
    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            if (diagnostic != nullptr)
            {
                *diagnostic = ec.message();
            }
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = "failed to open output file";
        }
        return false;
    }

    out << content;
    return static_cast<bool>(out);
}

[[nodiscard]] std::optional<std::string> ReadTextFile(const std::filesystem::path& path,
                                                      std::string* diagnostic)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = "failed to open input file";
        }
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    return content;
}

} // namespace

const char* ToString(CollaborationArtifactKind kind) noexcept
{
    switch (kind)
    {
    case CollaborationArtifactKind::ProjectMetadata:
        return "ProjectMetadata";
    case CollaborationArtifactKind::SagaScriptArtifactReview:
        return "SagaScriptArtifactReview";
    case CollaborationArtifactKind::PatchPreviewReview:
        return "PatchPreviewReview";
    case CollaborationArtifactKind::DiagnosticsReview:
        return "DiagnosticsReview";
    case CollaborationArtifactKind::Notes:
        return "Notes";
    case CollaborationArtifactKind::Unsupported:
        return "Unsupported";
    }
    return "Unsupported";
}

const char* ToString(CollaborationOperationKind kind) noexcept
{
    switch (kind)
    {
    case CollaborationOperationKind::ReviewPatchPreview:
        return "ReviewPatchPreview";
    case CollaborationOperationKind::MarkArtifactReviewed:
        return "MarkArtifactReviewed";
    case CollaborationOperationKind::AddNote:
        return "AddNote";
    case CollaborationOperationKind::ResolveNote:
        return "ResolveNote";
    case CollaborationOperationKind::AcknowledgeDiagnostic:
        return "AcknowledgeDiagnostic";
    case CollaborationOperationKind::CursorPresenceMoved:
        return "CursorPresenceMoved";
    case CollaborationOperationKind::Unsupported:
        return "Unsupported";
    }
    return "Unsupported";
}

const char* ToString(CollaborationTransactionStatus status) noexcept
{
    switch (status)
    {
    case CollaborationTransactionStatus::Accepted:
        return "Accepted";
    case CollaborationTransactionStatus::Rejected:
        return "Rejected";
    }
    return "Rejected";
}

const char* ToString(CollaborationConflictCategory category) noexcept
{
    switch (category)
    {
    case CollaborationConflictCategory::SamePatchPreviewReviewedByMultipleActors:
        return "SamePatchPreviewReviewedByMultipleActors";
    case CollaborationConflictCategory::StaleSourceHash:
        return "StaleSourceHash";
    case CollaborationConflictCategory::MissingTargetArtifact:
        return "MissingTargetArtifact";
    case CollaborationConflictCategory::UnsupportedOperation:
        return "UnsupportedOperation";
    case CollaborationConflictCategory::OutOfOrderOperation:
        return "OutOfOrderOperation";
    case CollaborationConflictCategory::UnknownActor:
        return "UnknownActor";
    case CollaborationConflictCategory::LockedArtifact:
        return "LockedArtifact";
    }
    return "UnsupportedOperation";
}

const char* ToString(CollaborationTargetKind kind) noexcept
{
    switch (kind)
    {
    case CollaborationTargetKind::ArtifactPath:
        return "ArtifactPath";
    case CollaborationTargetKind::BehaviorId:
        return "BehaviorId";
    case CollaborationTargetKind::NodeId:
        return "NodeId";
    case CollaborationTargetKind::SourceSpan:
        return "SourceSpan";
    case CollaborationTargetKind::PatchReport:
        return "PatchReport";
    case CollaborationTargetKind::DiagnosticId:
        return "DiagnosticId";
    case CollaborationTargetKind::Unsupported:
        return "Unsupported";
    }
    return "Unsupported";
}

const char* ToString(PersistentLockStatus status) noexcept
{
    switch (status)
    {
    case PersistentLockStatus::Active:
        return "Active";
    case PersistentLockStatus::Released:
        return "Released";
    case PersistentLockStatus::Stale:
        return "Stale";
    case PersistentLockStatus::Conflict:
        return "Conflict";
    }
    return "Conflict";
}

const char* ToString(ArtifactCommentStatus status) noexcept
{
    switch (status)
    {
    case ArtifactCommentStatus::Open:
        return "Open";
    case ArtifactCommentStatus::Resolved:
        return "Resolved";
    case ArtifactCommentStatus::Reopened:
        return "Reopened";
    }
    return "Open";
}

const char* ToString(ReviewRequestStatus status) noexcept
{
    switch (status)
    {
    case ReviewRequestStatus::Requested:
        return "Requested";
    case ReviewRequestStatus::InReview:
        return "InReview";
    case ReviewRequestStatus::ApprovedMetadataOnly:
        return "ApprovedMetadataOnly";
    case ReviewRequestStatus::Rejected:
        return "Rejected";
    case ReviewRequestStatus::Blocked:
        return "Blocked";
    case ReviewRequestStatus::Stale:
        return "Stale";
    case ReviewRequestStatus::Superseded:
        return "Superseded";
    }
    return "Blocked";
}

const char* ToString(TeamRole role) noexcept
{
    switch (role)
    {
    case TeamRole::Owner:
        return "Owner";
    case TeamRole::Programmer:
        return "Programmer";
    case TeamRole::Designer:
        return "Designer";
    case TeamRole::Artist:
        return "Artist";
    case TeamRole::QA:
        return "QA";
    case TeamRole::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

const char* ToString(DangerousOperationKind operation) noexcept
{
    switch (operation)
    {
    case DangerousOperationKind::DeleteScene:
        return "DeleteScene";
    case DangerousOperationKind::DeleteBehaviorSource:
        return "DeleteBehaviorSource";
    case DangerousOperationKind::ChangePackageProfile:
        return "ChangePackageProfile";
    case DangerousOperationKind::OverrideLock:
        return "OverrideLock";
    case DangerousOperationKind::ApprovePublishGate:
        return "ApprovePublishGate";
    case DangerousOperationKind::ModifyRuntimeBindingMetadataManually:
        return "ModifyRuntimeBindingMetadataManually";
    case DangerousOperationKind::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

const char* ToString(DangerousOperationCheckStatus status) noexcept
{
    switch (status)
    {
    case DangerousOperationCheckStatus::AllowedByLocalMetadata:
        return "AllowedByLocalMetadata";
    case DangerousOperationCheckStatus::BlockedByLocalMetadata:
        return "BlockedByLocalMetadata";
    case DangerousOperationCheckStatus::UnknownActor:
        return "UnknownActor";
    case DangerousOperationCheckStatus::UnknownOperation:
        return "UnknownOperation";
    }
    return "BlockedByLocalMetadata";
}

bool IsValid(const CollaborationActor& actor) noexcept
{
    return !actor.actorId.empty();
}

bool IsValid(const WorkspaceIdentity& identity) noexcept
{
    return identity.schemaVersion == 0 &&
           !identity.workspaceId.empty() &&
           !identity.actorId.empty();
}

bool IsValid(const CollaborationWorkspaceState& state) noexcept
{
    return state.schemaVersion == 1 &&
           state.tool == "SagaCollaboration" &&
           state.command == "workspace-state" &&
           !state.workspaceId.empty() &&
           !state.projectId.empty();
}

bool IsLockableArtifactKind(CollaborationArtifactKind kind) noexcept
{
    return kind == CollaborationArtifactKind::SagaScriptArtifactReview ||
           kind == CollaborationArtifactKind::PatchPreviewReview ||
           kind == CollaborationArtifactKind::DiagnosticsReview ||
           kind == CollaborationArtifactKind::Notes;
}

bool IsSemanticTransactionOperation(CollaborationOperationKind operation) noexcept
{
    return operation == CollaborationOperationKind::ReviewPatchPreview ||
           operation == CollaborationOperationKind::MarkArtifactReviewed ||
           operation == CollaborationOperationKind::AddNote ||
           operation == CollaborationOperationKind::ResolveNote ||
           operation == CollaborationOperationKind::AcknowledgeDiagnostic;
}

bool IsReviewApprovalMetadataOnly(ReviewRequestStatus status) noexcept
{
    return status == ReviewRequestStatus::ApprovedMetadataOnly;
}

std::string MakeArtifactResourceId(CollaborationArtifactKind kind, const std::string& path)
{
    return std::string(ToString(kind)) + ":" + path;
}

std::string MakeTargetResourceId(const CollaborationTarget& target)
{
    switch (target.kind)
    {
    case CollaborationTargetKind::ArtifactPath:
        return "artifact:" + target.artifactPath;
    case CollaborationTargetKind::BehaviorId:
        return "behavior:" + target.behaviorId;
    case CollaborationTargetKind::NodeId:
        return "node:" + target.nodeId;
    case CollaborationTargetKind::SourceSpan:
        return "source:" + target.sourcePath + ":" +
               std::to_string(target.startByte) + "-" +
               std::to_string(target.endByte);
    case CollaborationTargetKind::PatchReport:
        return "patch-report:" + target.reportPath;
    case CollaborationTargetKind::DiagnosticId:
        return "diagnostic:" + target.diagnosticId;
    case CollaborationTargetKind::Unsupported:
        return "unsupported";
    }
    return "unsupported";
}

std::string MakeLocalWorkspaceSessionId(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const CollaborationActor& actor)
{
    return "local-workspace:" + workspace.workspaceId + ":" + actor.actorId;
}

WorkspaceIdentity MakeWorkspaceIdentity(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const CollaborationActor& actor,
    std::string createdByTool)
{
    WorkspaceIdentity identity;
    identity.schemaVersion = 0;
    identity.workspaceId = workspace.workspaceId;
    identity.actorId = actor.actorId;
    identity.displayName = actor.displayName;
    identity.rolePlaceholder = actor.rolePlaceholder;
    identity.createdByTool = std::move(createdByTool);
    return identity;
}

LocalWorkspaceSession MakeLocalWorkspaceSession(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    CollaborationActor actor)
{
    LocalWorkspaceSession session;
    session.sessionId = MakeLocalWorkspaceSessionId(workspace, actor);
    session.workspaceId = workspace.workspaceId;
    session.projectId = workspace.projectId;
    session.actor = std::move(actor);
    session.readOnlyMetadataMode = true;
    return session;
}

std::string SerializeWorkspaceIdentityJson(const WorkspaceIdentity& identity)
{
    const Json value{
        {"schemaVersion", identity.schemaVersion},
        {"workspaceId", identity.workspaceId},
        {"actorId", identity.actorId},
        {"displayName", identity.displayName},
        {"rolePlaceholder", identity.rolePlaceholder},
        {"createdByTool", identity.createdByTool},
    };
    return value.dump(2) + "\n";
}

std::optional<WorkspaceIdentity> ParseWorkspaceIdentityJson(const std::string& content,
                                                            std::string* diagnostic)
{
    try
    {
        const Json value = Json::parse(content);
        WorkspaceIdentity identity;
        identity.schemaVersion = value.value("schemaVersion", 0U);
        identity.workspaceId = value.value("workspaceId", "");
        identity.actorId = value.value("actorId", "");
        identity.displayName = value.value("displayName", "");
        identity.rolePlaceholder = value.value("rolePlaceholder", "");
        identity.createdByTool = value.value("createdByTool", "");
        if (!IsValid(identity))
        {
            if (diagnostic != nullptr)
            {
                *diagnostic = "invalid workspace identity";
            }
            return std::nullopt;
        }
        return identity;
    }
    catch (const std::exception& ex)
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = ex.what();
        }
        return std::nullopt;
    }
}

bool WriteWorkspaceIdentity(const std::filesystem::path& path,
                            const WorkspaceIdentity& identity,
                            std::string* diagnostic)
{
    if (!IsValid(identity))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = "invalid workspace identity";
        }
        return false;
    }
    return WriteTextFile(path, SerializeWorkspaceIdentityJson(identity), diagnostic);
}

std::optional<WorkspaceIdentity> ReadWorkspaceIdentity(const std::filesystem::path& path,
                                                       std::string* diagnostic)
{
    const std::optional<std::string> content = ReadTextFile(path, diagnostic);
    if (!content.has_value())
    {
        return std::nullopt;
    }
    return ParseWorkspaceIdentityJson(*content, diagnostic);
}

std::string SerializeWorkspaceStateJson(const CollaborationWorkspaceState& state)
{
    Json actors = Json::array();
    for (const CollaborationActor& actor : state.actors)
    {
        actors.push_back(ActorToJson(actor));
    }

    Json sessions = Json::array();
    for (const LocalWorkspaceSession& session : state.sessions)
    {
        sessions.push_back(SessionToJson(session));
    }

    Json activeArtifactMemory = Json::array();
    for (const ArtifactPresence& presence : state.activeArtifactMemory)
    {
        activeArtifactMemory.push_back(PresenceToJson(presence));
    }

    Json diagnostics = Json::array();
    for (const auto& diagnostic : state.diagnostics)
    {
        diagnostics.push_back(DiagnosticToJson(diagnostic));
    }

    const Json report{
        {"schemaVersion", state.schemaVersion},
        {"tool", state.tool},
        {"command", state.command},
        {"status", state.status},
        {"workspaceId", state.workspaceId},
        {"projectId", state.projectId},
        {"actors", actors},
        {"sessions", sessions},
        {"activeArtifactMemory", activeArtifactMemory},
        {"diagnostics", diagnostics},
    };
    return report.dump(2) + "\n";
}

std::optional<CollaborationWorkspaceState> ParseWorkspaceStateJson(
    const std::string& content,
    std::string* diagnostic)
{
    try
    {
        const Json value = Json::parse(content);
        CollaborationWorkspaceState state;
        state.schemaVersion = value.value("schemaVersion", 0U);
        state.tool = value.value("tool", "");
        state.command = value.value("command", "");
        state.status = value.value("status", "");
        state.workspaceId = value.value("workspaceId", "");
        state.projectId = value.value("projectId", "");

        if (value.contains("actors") && value["actors"].is_array())
        {
            for (const Json& actor : value["actors"])
            {
                state.actors.push_back(ActorFromJson(actor));
            }
        }
        if (value.contains("sessions") && value["sessions"].is_array())
        {
            for (const Json& session : value["sessions"])
            {
                state.sessions.push_back(SessionFromJson(session));
            }
        }
        if (value.contains("activeArtifactMemory") && value["activeArtifactMemory"].is_array())
        {
            for (const Json& presence : value["activeArtifactMemory"])
            {
                state.activeArtifactMemory.push_back(PresenceFromJson(presence));
            }
        }

        if (!IsValid(state))
        {
            if (diagnostic != nullptr)
            {
                *diagnostic = "invalid collaboration workspace state";
            }
            return std::nullopt;
        }
        return state;
    }
    catch (const std::exception& ex)
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = ex.what();
        }
        return std::nullopt;
    }
}

bool WriteWorkspaceState(const std::filesystem::path& path,
                         const CollaborationWorkspaceState& state,
                         std::string* diagnostic)
{
    if (!IsValid(state))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = "invalid collaboration workspace state";
        }
        return false;
    }
    return WriteTextFile(path, SerializeWorkspaceStateJson(state), diagnostic);
}

std::optional<CollaborationWorkspaceState> ReadWorkspaceState(
    const std::filesystem::path& path,
    std::string* diagnostic)
{
    const std::optional<std::string> content = ReadTextFile(path, diagnostic);
    if (!content.has_value())
    {
        return std::nullopt;
    }
    return ParseWorkspaceStateJson(*content, diagnostic);
}

std::string SerializePresenceReportJson(const std::vector<ArtifactPresence>& presence)
{
    Json entries = Json::array();
    for (const ArtifactPresence& item : presence)
    {
        entries.push_back(PresenceToJson(item));
    }

    const Json report{
        {"schemaVersion", 0},
        {"ephemeral", true},
        {"presence", entries},
    };
    return report.dump(2) + "\n";
}

bool WritePresenceReport(const std::filesystem::path& path,
                         const std::vector<ArtifactPresence>& presence,
                         std::string* diagnostic)
{
    return WriteTextFile(path, SerializePresenceReportJson(presence), diagnostic);
}

PersistentArtifactLock MakePersistentLock(
    SagaShared::Collaboration::ResourceLock lock,
    CollaborationTarget target,
    std::uint64_t sequence)
{
    if (lock.resourceId.empty())
    {
        lock.resourceId = MakeTargetResourceId(target);
    }

    PersistentArtifactLock record;
    record.lock = std::move(lock);
    record.target = std::move(target);
    record.sequence = sequence;
    record.status = PersistentLockStatus::Active;
    return record;
}

PersistentLockStatus ClassifyPersistentLock(
    const PersistentArtifactLock& lock,
    std::uint64_t nowUnixMs) noexcept
{
    if (lock.status == PersistentLockStatus::Released ||
        lock.status == PersistentLockStatus::Conflict)
    {
        return lock.status;
    }
    if (lock.lock.expiresAtUnixMs != 0 && lock.lock.expiresAtUnixMs <= nowUnixMs)
    {
        return PersistentLockStatus::Stale;
    }
    return PersistentLockStatus::Active;
}

std::string SerializePersistentLocksReportJson(
    const std::vector<PersistentArtifactLock>& locks)
{
    Json entries = Json::array();
    for (const PersistentArtifactLock& lock : locks)
    {
        entries.push_back(PersistentLockToJson(lock));
    }

    const Json report{
        {"schemaVersion", 1},
        {"tool", "SagaCollaboration"},
        {"command", "persistent-locks"},
        {"status", "Passed"},
        {"locks", entries},
        {"diagnostics", Json::array()},
    };
    return report.dump(2) + "\n";
}

ArtifactComment MakeArtifactComment(
    std::string commentId,
    std::string threadId,
    std::string authorActorId,
    CollaborationTarget target,
    std::string body,
    std::uint64_t sequence)
{
    ArtifactComment comment;
    comment.commentId = std::move(commentId);
    comment.threadId = std::move(threadId);
    comment.authorActorId = std::move(authorActorId);
    comment.target = std::move(target);
    comment.body = std::move(body);
    comment.sequence = sequence;
    comment.status = ArtifactCommentStatus::Open;
    return comment;
}

void ResolveComment(ArtifactComment& comment) noexcept
{
    comment.status = ArtifactCommentStatus::Resolved;
}

void ReopenComment(ArtifactComment& comment) noexcept
{
    comment.status = ArtifactCommentStatus::Reopened;
}

std::string SerializeCommentsReportJson(const std::vector<ArtifactComment>& comments)
{
    Json entries = Json::array();
    for (const ArtifactComment& comment : comments)
    {
        entries.push_back(CommentToJson(comment));
    }

    const Json report{
        {"schemaVersion", 1},
        {"tool", "SagaCollaboration"},
        {"command", "artifact-comments"},
        {"status", "Passed"},
        {"comments", entries},
        {"diagnostics", Json::array()},
    };
    return report.dump(2) + "\n";
}

ReviewRequest MakeReviewRequest(
    std::string reviewId,
    std::string requesterActorId,
    std::vector<std::string> requestedReviewerActorIds,
    CollaborationTarget target,
    std::uint64_t sequence)
{
    ReviewRequest review;
    review.reviewId = std::move(reviewId);
    review.requesterActorId = std::move(requesterActorId);
    review.requestedReviewerActorIds = std::move(requestedReviewerActorIds);
    review.target = std::move(target);
    review.sequence = sequence;
    review.status = ReviewRequestStatus::Requested;
    return review;
}

void TransitionReviewRequest(ReviewRequest& review, ReviewRequestStatus status) noexcept
{
    review.status = status;
}

void RefreshReviewFreshness(ReviewRequest& review) noexcept
{
    if (!review.expectedSourceHash.empty() &&
        review.expectedSourceHash != review.actualSourceHash)
    {
        review.status = ReviewRequestStatus::Stale;
    }
    if (review.target.kind == CollaborationTargetKind::Unsupported)
    {
        review.status = ReviewRequestStatus::Blocked;
    }
}

std::string SerializeReviewReportJson(const std::vector<ReviewRequest>& reviews)
{
    Json entries = Json::array();
    for (const ReviewRequest& review : reviews)
    {
        entries.push_back(ReviewToJson(review));
    }

    const Json report{
        {"schemaVersion", 1},
        {"tool", "SagaCollaboration"},
        {"command", "review-requests"},
        {"status", "Passed"},
        {"reviews", entries},
        {"diagnostics", Json::array()},
    };
    return report.dump(2) + "\n";
}

DangerousOperationCheck CheckDangerousOperation(
    std::string operationId,
    const std::string& actorId,
    const std::vector<RoleAssignment>& roles,
    DangerousOperationKind operation,
    std::string resourceId)
{
    DangerousOperationCheck check;
    check.operationId = std::move(operationId);
    check.actorId = actorId;
    check.operation = operation;
    check.resourceId = std::move(resourceId);

    const auto roleIt = std::find_if(roles.begin(), roles.end(),
        [&actorId](const RoleAssignment& role)
        {
            return role.actorId == actorId;
        });
    if (roleIt == roles.end())
    {
        check.status = DangerousOperationCheckStatus::UnknownActor;
        check.reason = "actor has no local role metadata";
        return check;
    }

    check.role = roleIt->role;
    if (operation == DangerousOperationKind::Unknown)
    {
        check.status = DangerousOperationCheckStatus::UnknownOperation;
        check.reason = "operation is not in the local dangerous-operation list";
        return check;
    }

    const bool allowed = check.role == TeamRole::Owner ||
        (check.role == TeamRole::Programmer &&
            operation != DangerousOperationKind::ApprovePublishGate &&
            operation != DangerousOperationKind::OverrideLock) ||
        (check.role == TeamRole::QA &&
            operation == DangerousOperationKind::ApprovePublishGate);

    check.status = allowed
        ? DangerousOperationCheckStatus::AllowedByLocalMetadata
        : DangerousOperationCheckStatus::BlockedByLocalMetadata;
    check.reason = allowed
        ? "allowed by local role metadata"
        : "blocked by local role metadata";
    return check;
}

std::string SerializeRoleCheckReportJson(
    const std::vector<DangerousOperationCheck>& checks)
{
    Json entries = Json::array();
    for (const DangerousOperationCheck& check : checks)
    {
        entries.push_back(DangerousOperationCheckToJson(check));
    }

    const Json report{
        {"schemaVersion", 1},
        {"tool", "SagaCollaboration"},
        {"command", "role-check"},
        {"status", "Passed"},
        {"checks", entries},
        {"diagnostics", Json::array()},
    };
    return report.dump(2) + "\n";
}

std::string SerializeTransactionJsonLine(const SemanticTransaction& transaction)
{
    return TransactionToJson(transaction).dump();
}

bool WriteTransactionLog(const std::filesystem::path& path,
                         const std::vector<SemanticTransaction>& transactions,
                         std::string* diagnostic)
{
    std::string content;
    for (const SemanticTransaction& transaction : transactions)
    {
        content += SerializeTransactionJsonLine(transaction);
        content += '\n';
    }
    return WriteTextFile(path, content, diagnostic);
}

std::string SerializeConflictReportJson(const std::vector<CollaborationConflict>& conflicts)
{
    Json entries = Json::array();
    for (const CollaborationConflict& conflict : conflicts)
    {
        entries.push_back(ConflictToJson(conflict));
    }

    const Json report{
        {"schemaVersion", 0},
        {"conflicts", entries},
    };
    return report.dump(2) + "\n";
}

bool WriteConflictReport(const std::filesystem::path& path,
                         const std::vector<CollaborationConflict>& conflicts,
                         std::string* diagnostic)
{
    return WriteTextFile(path, SerializeConflictReportJson(conflicts), diagnostic);
}

std::string SerializeTeamRoomDashboardJson(const TeamRoomDashboardView& dashboard)
{
    Json actors = Json::array();
    for (const CollaborationActor& actor : dashboard.actors)
    {
        actors.push_back(ActorToJson(actor));
    }

    Json presence = Json::array();
    for (const ArtifactPresence& item : dashboard.presence)
    {
        presence.push_back(PresenceToJson(item));
    }

    Json locks = Json::array();
    for (const SagaShared::Collaboration::ResourceLock& lock : dashboard.locks)
    {
        locks.push_back(LockToJson(lock));
    }

    Json transactions = Json::array();
    for (const SemanticTransaction& transaction : dashboard.recentTransactions)
    {
        transactions.push_back(TransactionToJson(transaction));
    }

    Json conflicts = Json::array();
    for (const CollaborationConflict& conflict : dashboard.conflicts)
    {
        conflicts.push_back(ConflictToJson(conflict));
    }

    Json diagnostics = Json::array();
    for (const CollaborationArtifactRecord& artifact : dashboard.diagnosticsReviewStatus)
    {
        diagnostics.push_back(ArtifactToJson(artifact));
    }

    Json comments = Json::array();
    for (const ArtifactComment& comment : dashboard.comments)
    {
        comments.push_back(CommentToJson(comment));
    }

    Json reviews = Json::array();
    for (const ReviewRequest& review : dashboard.reviewQueue)
    {
        reviews.push_back(ReviewToJson(review));
    }

    Json dangerousChecks = Json::array();
    for (const DangerousOperationCheck& check : dashboard.dangerousOperationChecks)
    {
        dangerousChecks.push_back(DangerousOperationCheckToJson(check));
    }

    const Json report{
        {"schemaVersion", 1},
        {"tool", "SagaCollaboration"},
        {"command", "team-room-status"},
        {"status", "Passed"},
        {"actors", actors},
        {"presence", presence},
        {"locks", locks},
        {"recentTransactions", transactions},
        {"conflicts", conflicts},
        {"diagnosticsReviewStatus", diagnostics},
        {"comments", comments},
        {"reviewQueue", reviews},
        {"dangerousOperationChecks", dangerousChecks},
        {"packageStatusPlaceholder", dashboard.packageStatusPlaceholder},
        {"publishStatusPlaceholder", dashboard.publishStatusPlaceholder},
        {"diagnostics", Json::array()},
    };
    return report.dump(2) + "\n";
}

bool LocalCollaborationMetadataStore::RegisterActor(CollaborationActor actor)
{
    if (!IsValid(actor))
    {
        return false;
    }

    auto it = std::find_if(m_actors.begin(), m_actors.end(),
        [&actor](const CollaborationActor& item)
        {
            return item.actorId == actor.actorId;
        });
    if (it == m_actors.end())
    {
        m_actors.push_back(std::move(actor));
        return true;
    }

    *it = std::move(actor);
    return true;
}

void LocalCollaborationMetadataStore::RegisterArtifact(CollaborationArtifactRecord artifact)
{
    auto it = std::find_if(m_artifacts.begin(), m_artifacts.end(),
        [&artifact](const CollaborationArtifactRecord& item)
        {
            return item.kind == artifact.kind && item.path == artifact.path;
        });
    if (it == m_artifacts.end())
    {
        m_artifacts.push_back(std::move(artifact));
        return;
    }

    *it = std::move(artifact);
}

void LocalCollaborationMetadataStore::UpdatePresence(ArtifactPresence presence)
{
    auto it = std::find_if(m_presence.begin(), m_presence.end(),
        [&presence](const ArtifactPresence& item)
        {
            return item.actorId == presence.actorId;
        });
    if (it == m_presence.end())
    {
        m_presence.push_back(std::move(presence));
        return;
    }

    *it = std::move(presence);
}

bool LocalCollaborationMetadataStore::TryAcquireLock(SagaShared::Collaboration::ResourceLock lock)
{
    if (lock.resourceId.empty())
    {
        return false;
    }

    const SagaShared::Collaboration::ResourceLock* existing = FindLock(lock.resourceId);
    if (existing != nullptr && existing->lockId != lock.lockId)
    {
        return false;
    }

    ReleaseLock(lock.lockId);
    m_locks.push_back(std::move(lock));
    return true;
}

void LocalCollaborationMetadataStore::ReleaseLock(const std::string& lockId)
{
    m_locks.erase(
        std::remove_if(m_locks.begin(), m_locks.end(),
            [&lockId](const SagaShared::Collaboration::ResourceLock& item)
            {
                return item.lockId == lockId;
            }),
        m_locks.end());
}

void LocalCollaborationMetadataStore::RegisterComment(ArtifactComment comment)
{
    auto it = std::find_if(m_comments.begin(), m_comments.end(),
        [&comment](const ArtifactComment& item)
        {
            return item.commentId == comment.commentId;
        });
    if (it == m_comments.end())
    {
        m_comments.push_back(std::move(comment));
        return;
    }

    *it = std::move(comment);
}

void LocalCollaborationMetadataStore::RegisterReviewRequest(ReviewRequest review)
{
    auto it = std::find_if(m_reviews.begin(), m_reviews.end(),
        [&review](const ReviewRequest& item)
        {
            return item.reviewId == review.reviewId;
        });
    if (it == m_reviews.end())
    {
        m_reviews.push_back(std::move(review));
        return;
    }

    *it = std::move(review);
}

void LocalCollaborationMetadataStore::RegisterDangerousOperationCheck(
    DangerousOperationCheck check)
{
    auto it = std::find_if(m_dangerousOperationChecks.begin(), m_dangerousOperationChecks.end(),
        [&check](const DangerousOperationCheck& item)
        {
            return item.operationId == check.operationId;
        });
    if (it == m_dangerousOperationChecks.end())
    {
        m_dangerousOperationChecks.push_back(std::move(check));
        return;
    }

    *it = std::move(check);
}

TransactionSubmission LocalCollaborationMetadataStore::SubmitTransaction(
    SemanticTransaction transaction)
{
    if (!IsSemanticTransactionOperation(transaction.operationKind))
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::UnsupportedOperation,
                      "operation is not a supported metadata transaction");
    }

    if (transaction.sequence != m_nextSequence)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::OutOfOrderOperation,
                      "operation sequence is not the next expected value");
    }

    if (!HasActor(transaction.actorId))
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::UnknownActor,
                      "operation actor is not registered in local workspace identity");
    }

    const CollaborationArtifactRecord* artifact =
        FindArtifact(transaction.targetArtifactKind, transaction.targetPath);
    if (artifact == nullptr)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::MissingTargetArtifact,
                      "operation target artifact is not registered");
    }

    const std::string resourceId =
        MakeArtifactResourceId(transaction.targetArtifactKind, transaction.targetPath);
    const SagaShared::Collaboration::ResourceLock* lock = FindLock(resourceId);
    if (lock != nullptr && lock->participant.value != transaction.actorId)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::LockedArtifact,
                      "operation target artifact is locked by another actor");
    }

    if (transaction.baseSourceHash.has_value() &&
        *transaction.baseSourceHash != artifact->sourceHash)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::StaleSourceHash,
                      "operation base source hash does not match target artifact");
    }

    if (transaction.operationKind == CollaborationOperationKind::ReviewPatchPreview)
    {
        const auto it = std::find_if(m_transactions.begin(), m_transactions.end(),
            [&transaction](const SemanticTransaction& item)
            {
                return item.operationKind == CollaborationOperationKind::ReviewPatchPreview &&
                       item.targetArtifactKind == transaction.targetArtifactKind &&
                       item.targetPath == transaction.targetPath &&
                       item.actorId != transaction.actorId;
            });
        if (it != m_transactions.end())
        {
            return Reject(std::move(transaction),
                          CollaborationConflictCategory::SamePatchPreviewReviewedByMultipleActors,
                          "patch preview was already reviewed by another actor");
        }
    }

    transaction.status = CollaborationTransactionStatus::Accepted;
    m_transactions.push_back(transaction);
    ++m_nextSequence;
    return TransactionSubmission{transaction, true, std::nullopt};
}

bool LocalCollaborationMetadataStore::RecordPresenceGestureAsTransaction(
    const ArtifactPresence& presence)
{
    UpdatePresence(presence);
    return false;
}

const std::vector<CollaborationActor>& LocalCollaborationMetadataStore::Actors() const noexcept
{
    return m_actors;
}

const std::vector<ArtifactPresence>& LocalCollaborationMetadataStore::Presence() const noexcept
{
    return m_presence;
}

const std::vector<SagaShared::Collaboration::ResourceLock>&
LocalCollaborationMetadataStore::Locks() const noexcept
{
    return m_locks;
}

const std::vector<ArtifactComment>&
LocalCollaborationMetadataStore::Comments() const noexcept
{
    return m_comments;
}

const std::vector<ReviewRequest>&
LocalCollaborationMetadataStore::ReviewRequests() const noexcept
{
    return m_reviews;
}

const std::vector<DangerousOperationCheck>&
LocalCollaborationMetadataStore::DangerousOperationChecks() const noexcept
{
    return m_dangerousOperationChecks;
}

const std::vector<SemanticTransaction>&
LocalCollaborationMetadataStore::Transactions() const noexcept
{
    return m_transactions;
}

const std::vector<CollaborationConflict>&
LocalCollaborationMetadataStore::Conflicts() const noexcept
{
    return m_conflicts;
}

TeamRoomDashboardView LocalCollaborationMetadataStore::BuildDashboardView() const
{
    TeamRoomDashboardView dashboard;
    dashboard.actors = m_actors;
    dashboard.presence = m_presence;
    dashboard.locks = m_locks;
    dashboard.recentTransactions = m_transactions;
    dashboard.conflicts = m_conflicts;
    dashboard.comments = m_comments;
    dashboard.reviewQueue = m_reviews;
    dashboard.dangerousOperationChecks = m_dangerousOperationChecks;
    dashboard.packageStatusPlaceholder = "Deferred";
    dashboard.publishStatusPlaceholder = "Deferred";

    for (const CollaborationArtifactRecord& artifact : m_artifacts)
    {
        if (artifact.kind == CollaborationArtifactKind::DiagnosticsReview)
        {
            dashboard.diagnosticsReviewStatus.push_back(artifact);
        }
    }
    return dashboard;
}

bool LocalCollaborationMetadataStore::HasActor(const std::string& actorId) const
{
    return std::any_of(m_actors.begin(), m_actors.end(),
        [&actorId](const CollaborationActor& actor)
        {
            return actor.actorId == actorId;
        });
}

const CollaborationArtifactRecord* LocalCollaborationMetadataStore::FindArtifact(
    CollaborationArtifactKind kind,
    const std::string& path) const
{
    auto it = std::find_if(m_artifacts.begin(), m_artifacts.end(),
        [kind, &path](const CollaborationArtifactRecord& artifact)
        {
            return artifact.kind == kind && artifact.path == path;
        });
    return it == m_artifacts.end() ? nullptr : &(*it);
}

const SagaShared::Collaboration::ResourceLock* LocalCollaborationMetadataStore::FindLock(
    const std::string& resourceId) const
{
    auto it = std::find_if(m_locks.begin(), m_locks.end(),
        [&resourceId](const SagaShared::Collaboration::ResourceLock& lock)
        {
            return lock.resourceId == resourceId;
        });
    return it == m_locks.end() ? nullptr : &(*it);
}

CollaborationConflict LocalCollaborationMetadataStore::MakeConflict(
    const SemanticTransaction& transaction,
    CollaborationConflictCategory category,
    std::string reason) const
{
    CollaborationConflict conflict;
    conflict.conflictId = "conflict:" + transaction.operationId + ":" + ToString(category);
    conflict.category = category;
    conflict.operationId = transaction.operationId;
    conflict.actorId = transaction.actorId;
    conflict.targetArtifactKind = transaction.targetArtifactKind;
    conflict.targetPath = transaction.targetPath;
    conflict.reason = std::move(reason);
    return conflict;
}

TransactionSubmission LocalCollaborationMetadataStore::Reject(
    SemanticTransaction transaction,
    CollaborationConflictCategory category,
    std::string reason)
{
    transaction.status = CollaborationTransactionStatus::Rejected;
    CollaborationConflict conflict = MakeConflict(transaction, category, std::move(reason));
    m_conflicts.push_back(conflict);
    return TransactionSubmission{transaction, false, conflict};
}

} // namespace SagaCollaboration
