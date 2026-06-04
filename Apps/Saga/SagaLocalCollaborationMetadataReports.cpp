/// @file SagaLocalCollaborationMetadataReports.cpp
/// @brief Report-only local collaboration metadata boundary proofs.

#include "SagaLocalCollaborationMetadataReports.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace SagaProduct
{
namespace
{

namespace fs = std::filesystem;

struct ProjectMetadata
{
    bool ok = false;
    std::string projectId;
    std::string displayName;
    fs::path manifestPath;
    fs::path projectRoot;
    std::vector<nlohmann::json> diagnostics;
};

[[nodiscard]] nlohmann::json Diagnostic(std::string code,
                                        std::string message,
                                        const fs::path& path = {})
{
    nlohmann::json diagnostic;
    diagnostic["code"] = std::move(code);
    diagnostic["message"] = std::move(message);
    diagnostic["path"] = path.empty() ? "" : path.string();
    return diagnostic;
}

[[nodiscard]] ProjectMetadata LoadProjectMetadata(const fs::path& inputPath)
{
    ProjectMetadata metadata;
    metadata.manifestPath = fs::absolute(inputPath);
    metadata.projectRoot = metadata.manifestPath.parent_path();

    if (inputPath.empty())
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ProjectMissing",
            "local collaboration metadata report requires --project <.sagaproj>"));
        return metadata;
    }
    if (!fs::exists(metadata.manifestPath))
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ProjectManifestMissing",
            "project manifest is missing",
            metadata.manifestPath));
        return metadata;
    }

    std::ifstream input(metadata.manifestPath, std::ios::binary);
    if (!input.is_open())
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ProjectManifestUnreadable",
            "project manifest could not be opened",
            metadata.manifestPath));
        return metadata;
    }

    try
    {
        const nlohmann::json manifest = nlohmann::json::parse(input);
        metadata.projectId = manifest.value("projectId", std::string{});
        metadata.displayName = manifest.value("displayName", std::string{});
        metadata.ok = !metadata.projectId.empty();
        if (!metadata.ok)
        {
            metadata.diagnostics.push_back(Diagnostic(
                "Saga.LocalCollaboration.ProjectIdMissing",
                "project manifest does not contain projectId",
                metadata.manifestPath));
        }
    }
    catch (const std::exception& ex)
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ProjectManifestParseFailed",
            ex.what(),
            metadata.manifestPath));
    }

    return metadata;
}

[[nodiscard]] std::string WorkspaceIdForSelector(const std::string& selector)
{
    if (selector.empty() || selector == "builtin:basic")
    {
        return "builtin.basic";
    }
    return selector;
}

[[nodiscard]] std::string OrDefault(const std::string& value,
                                    const char* fallback)
{
    return value.empty() ? std::string(fallback) : value;
}

[[nodiscard]] fs::path AbsoluteIfPresent(const fs::path& path)
{
    return path.empty() ? fs::path{} : fs::absolute(path).lexically_normal();
}

[[nodiscard]] std::string IdSegment(std::string value)
{
    for (char& ch : value)
    {
        const auto byte = static_cast<unsigned char>(ch);
        if (!std::isalnum(byte) && ch != '.' && ch != '_' && ch != '-')
        {
            ch = '-';
        }
    }
    return value.empty() ? "none" : value;
}

[[nodiscard]] std::string ProjectIdOrUnknown(const ProjectMetadata& project)
{
    return project.projectId.empty() ? "unknown-project" : project.projectId;
}

[[nodiscard]] std::string TargetSegment(const fs::path& targetPath)
{
    return IdSegment(targetPath.empty() ? std::string("missing-target") :
        targetPath.generic_string());
}

[[nodiscard]] std::string MakeLockId(const std::string& workspaceId,
                                     const std::string& projectId,
                                     const std::string& actorId,
                                     const fs::path& targetPath)
{
    return "local-lock:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeReviewId(const std::string& workspaceId,
                                       const std::string& projectId,
                                       const std::string& actorId,
                                       const fs::path& targetPath)
{
    return "local-review:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeCommentId(const std::string& workspaceId,
                                        const std::string& projectId,
                                        const std::string& actorId,
                                        const fs::path& targetPath)
{
    return "local-comment:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeAuditEventId(const std::string& workspaceId,
                                           const std::string& projectId,
                                           const std::string& actorId,
                                           const std::string& eventKind)
{
    return "local-audit:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        IdSegment(eventKind);
}

[[nodiscard]] std::string MakeRoleId(const std::string& workspaceId,
                                     const std::string& projectId,
                                     const std::string& actorId,
                                     const std::string& roleName)
{
    return "local-role:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        IdSegment(roleName);
}

[[nodiscard]] std::string MakePermissionId(const std::string& workspaceId,
                                           const std::string& projectId,
                                           const std::string& actorId,
                                           const std::string& permissionName,
                                           const fs::path& targetPath)
{
    return "local-permission:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        IdSegment(permissionName) + ":" + TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeSliceId(const std::string& workspaceId,
                                      const std::string& projectId,
                                      const std::string& actorId,
                                      const std::string& sliceName,
                                      const fs::path& targetPath)
{
    return "local-slice:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        IdSegment(sliceName) + ":" + TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeApprovalId(
    const std::string& workspaceId,
    const std::string& projectId,
    const std::string& actorId,
    const std::string& roleName,
    const std::string& approvalState,
    const fs::path& targetPath)
{
    return "local-approval:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        IdSegment(roleName) + ":" + IdSegment(approvalState) + ":" +
        TargetSegment(targetPath);
}

[[nodiscard]] std::string MakeApprovalGateId(
    const std::string& workspaceId,
    const std::string& projectId,
    const std::string& actorId,
    const fs::path& targetPath)
{
    return "local-approval-gate:" + IdSegment(workspaceId) + ":" +
        IdSegment(projectId) + ":" + IdSegment(actorId) + ":" +
        TargetSegment(targetPath);
}

[[nodiscard]] std::string ApprovalStateForInput(const std::string& state)
{
    if (state == "requested-local-preview" ||
        state == "RequestedLocalPreview")
    {
        return "RequestedLocalPreview";
    }
    if (state == "rejected-local-preview" ||
        state == "RejectedLocalPreview")
    {
        return "RejectedLocalPreview";
    }
    return "ApprovedLocalPreview";
}

[[nodiscard]] std::string BodyPreview(const std::string& body)
{
    constexpr std::size_t kMaxPreviewLength = 120;
    if (body.size() <= kMaxPreviewLength)
    {
        return body;
    }
    return body.substr(0, kMaxPreviewLength);
}

[[nodiscard]] std::vector<nlohmann::json> SharedNonClaims()
{
    return {
        "full multiplayer collaboration",
        "cloud workspace",
        "enterprise permissions",
        "enterprise access control",
        "real permission enforcement",
        "secure access control",
        "enterprise policy engine",
        "durable role service",
        "durable permission service",
        "networked authorization",
        "real-time team editing",
        "networked presence",
        "durable lock service",
        "durable audit service",
        "CRDT/OT",
        "collaboration server",
        "full team workspace",
        "approval workflow",
        "real approval workflow",
        "durable approval service",
        "actual publish blocker",
        "package readiness",
        "tamper-resistant audit log",
        "product beta",
        "distribution readiness",
    };
}

[[nodiscard]] std::vector<nlohmann::json> RolePermissionLimitations()
{
    return {
        "Local role and permission smoke is a no-UI report-only boundary proof.",
        "Role metadata is local to the report and is not durable project truth.",
        "Permission metadata is intent-only and is not enforced.",
        "The report does not authenticate the actor or provide secure access control.",
        "The report does not mutate project files, scenes, scripts, or SDE files.",
        "The report does not write durable collaboration metadata.",
        "Enterprise policy, cloud sync, CRDT/OT, and collaboration server work are deferred.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> ProjectSliceLimitations()
{
    return {
        "Local project slice smoke is a no-UI report-only boundary proof.",
        "Slice visibility metadata is local to the report and is not enforced.",
        "The report does not resolve .saga/slices or create restricted project truth.",
        "The report does not mutate project files, scenes, scripts, or SDE files.",
        "The report does not write durable project slice metadata.",
        "Enterprise policy, cloud sync, CRDT/OT, and collaboration server work are deferred.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> ApprovalGateLimitations()
{
    return {
        "Local approval gate smoke is a no-UI report-only boundary proof.",
        "Approval metadata is local to the report and is not a real approval workflow.",
        "Publish gate metadata is a preview and is not an actual publish blocker.",
        "Package preflight is represented as blocked and does not claim package readiness.",
        "Distribution readiness is always false for this report.",
        "The report does not enforce permissions or enterprise policy.",
        "The report does not mutate project files, scenes, scripts, SDE files, package profiles, diagnostics folders, report folders, workspace files, or package outputs.",
        "The report does not write durable approval, policy, audit, or collaboration metadata.",
        "Cloud sync, CRDT/OT, real-time team editing, and collaboration server work are deferred.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> PresenceLockLimitations()
{
    return {
        "Local presence and lock smoke is a no-UI report-only boundary proof.",
        "Presence metadata is local to the report and is not networked.",
        "Lock metadata is intent-only and is not a durable lock service.",
        "The report does not enforce permissions or block another actor.",
        "The report does not mutate project files, scenes, scripts, or SDE files.",
        "The report does not write durable collaboration metadata.",
        "Enterprise policy, cloud sync, CRDT/OT, and collaboration server work are deferred.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> ReviewAuditLimitations()
{
    return {
        "Local review and audit smoke is a no-UI report-only boundary proof.",
        "Review metadata is not an approval workflow.",
        "Comment metadata is local to the report and is not durable project truth.",
        "Audit metadata is not a durable or tamper-resistant audit log.",
        "The report does not mutate project files, scenes, scripts, or SDE files.",
        "The report does not write durable collaboration metadata.",
        "Enterprise policy, cloud sync, CRDT/OT, and collaboration server work are deferred.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] nlohmann::json ActorJson(const std::string& actorId)
{
    return {
        { "actorId", actorId },
        { "displayName", actorId },
        { "source", "report-only" },
    };
}

[[nodiscard]] nlohmann::json WorkspaceJson(const std::string& workspaceId,
                                           const std::string& selector)
{
    return {
        { "workspaceId", workspaceId },
        { "selector", selector },
    };
}

[[nodiscard]] nlohmann::json ProjectJson(const ProjectMetadata& project)
{
    return {
        { "projectId", project.projectId },
        { "displayName", project.displayName },
        { "manifestPath", project.manifestPath.string() },
        { "projectRoot", project.projectRoot.string() },
    };
}

[[nodiscard]] bool WriteJsonReport(const fs::path& reportPath,
                                   const nlohmann::json& report,
                                   std::string& error)
{
    std::error_code ec;
    if (!reportPath.parent_path().empty())
    {
        fs::create_directories(reportPath.parent_path(), ec);
    }
    std::ofstream output(reportPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open())
    {
        error = "unable to write local collaboration metadata report: " +
            reportPath.string();
        return false;
    }
    output << report.dump(2) << '\n';
    return true;
}

} // namespace

SagaLocalCollaborationMetadataReportResult WriteLocalPresenceLockReport(
    const SagaLocalPresenceLockReportRequest& request)
{
    SagaLocalCollaborationMetadataReportResult result;
    result.reportPath = request.reportPath;

    ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path lockTarget = AbsoluteIfPresent(request.lockTargetPath);
    if (lockTarget.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.LockTargetMissing",
            "local presence/lock smoke requires --lock-target <path>"));
    }

    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string projectId = ProjectIdOrUnknown(project);
    const bool ready = project.ok && !lockTarget.empty();
    result.status = ready ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-presence-lock-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = WorkspaceJson(workspaceId, workspaceSelector);
    report["project"] = ProjectJson(project);
    report["actor"] = ActorJson(actorId);
    report["presence"] = {
        { "actorId", actorId },
        { "displayName", actorId },
        { "status", "PresentLocal" },
        { "source", "report-only" },
        { "durable", false },
        { "networked", false },
    };
    report["lock"] = {
        { "lockId", MakeLockId(workspaceId, projectId, actorId, lockTarget) },
        { "targetArtifact", lockTarget.string() },
        { "lockMode", "ReadOnlyPreview" },
        { "status", result.status },
        { "conflictStatus", "NotChecked" },
        { "durable", false },
        { "mutatesProject", false },
    };
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = PresenceLockLimitations();
    report["nonClaims"] = SharedNonClaims();

    result.ok = WriteJsonReport(request.reportPath, report, result.error);
    return result;
}

SagaLocalCollaborationMetadataReportResult WriteLocalReviewAuditReport(
    const SagaLocalReviewAuditReportRequest& request)
{
    SagaLocalCollaborationMetadataReportResult result;
    result.reportPath = request.reportPath;

    ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path reviewTarget = AbsoluteIfPresent(request.reviewTargetPath);
    if (reviewTarget.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ReviewTargetMissing",
            "local review smoke requires --review-target <path>"));
    }

    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string projectId = ProjectIdOrUnknown(project);
    const std::string comment =
        OrDefault(request.comment, "Inspect project metadata");
    const std::string eventKind = "LocalReviewMetadataRecorded";
    const bool ready = project.ok && !reviewTarget.empty();
    result.status = ready ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-review-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = WorkspaceJson(workspaceId, workspaceSelector);
    report["project"] = ProjectJson(project);
    report["actor"] = ActorJson(actorId);
    report["review"] = {
        { "reviewId", MakeReviewId(workspaceId, projectId, actorId, reviewTarget) },
        { "targetArtifact", reviewTarget.string() },
        { "reviewMode", "MetadataOnly" },
        { "status", result.status },
        { "durable", false },
        { "requiresApproval", false },
        { "mutatesProject", false },
    };
    report["comment"] = {
        { "commentId", MakeCommentId(workspaceId, projectId, actorId, reviewTarget) },
        { "bodyPreview", BodyPreview(comment) },
        { "targetArtifact", reviewTarget.string() },
        { "source", "report-only" },
        { "durable", false },
    };
    report["auditEvent"] = {
        { "eventId", MakeAuditEventId(workspaceId, projectId, actorId, eventKind) },
        { "eventKind", eventKind },
        { "actorId", actorId },
        { "targetArtifact", reviewTarget.string() },
        { "source", "report-only" },
        { "durable", false },
        { "tamperResistant", false },
    };
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = ReviewAuditLimitations();
    report["nonClaims"] = SharedNonClaims();

    result.ok = WriteJsonReport(request.reportPath, report, result.error);
    return result;
}

SagaLocalCollaborationMetadataReportResult WriteLocalRolePermissionReport(
    const SagaLocalRolePermissionReportRequest& request)
{
    SagaLocalCollaborationMetadataReportResult result;
    result.reportPath = request.reportPath;

    ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path targetArtifact = AbsoluteIfPresent(project.manifestPath);
    if (request.roleName.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.RoleMissing",
            "local role smoke requires --role <name>"));
    }
    if (request.permissionName.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.PermissionMissing",
            "local role smoke requires --permission <name>"));
    }

    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string projectId = ProjectIdOrUnknown(project);
    const bool ready =
        project.ok && !request.roleName.empty() &&
        !request.permissionName.empty();
    result.status = ready ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-role-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = WorkspaceJson(workspaceId, workspaceSelector);
    report["project"] = ProjectJson(project);
    report["actor"] = ActorJson(actorId);
    report["role"] = {
        { "roleId", MakeRoleId(workspaceId, projectId, actorId, request.roleName) },
        { "actorId", actorId },
        { "roleName", request.roleName },
        { "source", "report-only" },
        { "durable", false },
        { "enforced", false },
        { "networked", false },
    };
    report["permission"] = {
        { "permissionId", MakePermissionId(workspaceId,
                                           projectId,
                                           actorId,
                                           request.permissionName,
                                           targetArtifact) },
        { "permissionName", request.permissionName },
        { "targetArtifact", targetArtifact.string() },
        { "status", ready ? "Represented" : "Failed" },
        { "enforced", false },
        { "policyBacked", false },
        { "mutatesProject", false },
    };
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = RolePermissionLimitations();
    report["nonClaims"] = SharedNonClaims();

    result.ok = WriteJsonReport(request.reportPath, report, result.error);
    return result;
}

SagaLocalCollaborationMetadataReportResult WriteLocalProjectSliceReport(
    const SagaLocalProjectSliceReportRequest& request)
{
    SagaLocalCollaborationMetadataReportResult result;
    result.reportPath = request.reportPath;

    ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path sliceTarget = AbsoluteIfPresent(request.sliceTargetPath);
    if (request.sliceName.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.SliceMissing",
            "local project slice smoke requires --slice <name>"));
    }
    if (sliceTarget.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.SliceTargetMissing",
            "local project slice smoke requires --slice-target <path>"));
    }

    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string projectId = ProjectIdOrUnknown(project);
    const bool ready =
        project.ok && !request.sliceName.empty() && !sliceTarget.empty();
    result.status = ready ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-slice-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = WorkspaceJson(workspaceId, workspaceSelector);
    report["project"] = ProjectJson(project);
    report["actor"] = ActorJson(actorId);
    report["slice"] = {
        { "sliceId", MakeSliceId(workspaceId,
                                 projectId,
                                 actorId,
                                 request.sliceName,
                                 sliceTarget) },
        { "displayName", request.sliceName },
        { "targetArtifact", sliceTarget.string() },
        { "sliceMode", "MetadataOnly" },
        { "status", ready ? "Represented" : "Failed" },
        { "durable", false },
        { "mutatesProject", false },
    };
    report["visibility"] = {
        { "visibleToActor", ready },
        { "visibilitySource", "report-only" },
        { "permissionEnforced", false },
        { "policyBacked", false },
        { "networked", false },
    };
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = ProjectSliceLimitations();
    report["nonClaims"] = SharedNonClaims();

    result.ok = WriteJsonReport(request.reportPath, report, result.error);
    return result;
}

SagaLocalCollaborationMetadataReportResult WriteLocalApprovalGateReport(
    const SagaLocalApprovalGateReportRequest& request)
{
    SagaLocalCollaborationMetadataReportResult result;
    result.reportPath = request.reportPath;

    ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path gateTarget = AbsoluteIfPresent(request.gateTargetPath);
    if (request.roleName.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.ApprovalRoleMissing",
            "local approval gate smoke requires --role <name>"));
    }
    if (gateTarget.empty())
    {
        project.diagnostics.push_back(Diagnostic(
            "Saga.LocalCollaboration.GateTargetMissing",
            "local approval gate smoke requires --gate-target <path>"));
    }

    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string projectId = ProjectIdOrUnknown(project);
    const std::string approvalState =
        ApprovalStateForInput(request.approvalState);
    const bool ready =
        project.ok && !request.roleName.empty() && !gateTarget.empty();
    result.status = ready ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-approval-gate-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = WorkspaceJson(workspaceId, workspaceSelector);
    report["project"] = ProjectJson(project);
    report["actor"] = ActorJson(actorId);
    report["approval"] = {
        { "approvalId", MakeApprovalId(workspaceId,
                                       projectId,
                                       actorId,
                                       request.roleName,
                                       approvalState,
                                       gateTarget) },
        { "actorId", actorId },
        { "roleName", request.roleName },
        { "targetArtifact", gateTarget.string() },
        { "approvalState", approvalState },
        { "source", "report-only" },
        { "durable", false },
        { "enforced", false },
        { "requiresServer", false },
        { "mutatesProject", false },
    };
    report["publishGate"] = {
        { "gateId", MakeApprovalGateId(workspaceId,
                                       projectId,
                                       actorId,
                                       gateTarget) },
        { "targetArtifact", gateTarget.string() },
        { "gateMode", "MetadataOnly" },
        { "status", ready ? "Blocked" : "Failed" },
        { "packagePreflightStatus", "Blocked" },
        { "distributionReady", false },
        { "policyBacked", false },
        { "enforced", false },
        { "durable", false },
        { "mutatesProject", false },
    };
    report["readiness"] = {
        { "canPublish", false },
        { "reason", "Package and distribution readiness are not implemented by this report." },
        { "blockingLimitations", std::vector<nlohmann::json>{
            "Package preflight is blocked in this metadata-only preview.",
            "Distribution readiness is not implemented.",
            "Approval intent is report-only and is not enforced.",
        } },
        { "referencedReports", std::vector<nlohmann::json>{
            "local-workspace-transaction-smoke",
            "local-workspace-presence-lock-smoke",
            "local-workspace-review-smoke",
            "local-workspace-role-smoke",
            "local-workspace-slice-smoke",
            "workflow-smoke package_preflight reference",
        } },
    };
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = ApprovalGateLimitations();
    report["nonClaims"] = SharedNonClaims();

    result.ok = WriteJsonReport(request.reportPath, report, result.error);
    return result;
}

} // namespace SagaProduct
