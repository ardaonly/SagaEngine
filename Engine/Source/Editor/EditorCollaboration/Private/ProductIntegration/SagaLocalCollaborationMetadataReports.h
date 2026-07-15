/// @file SagaLocalCollaborationMetadataReports.h
/// @brief Report-only local collaboration metadata boundary proofs.

#pragma once

#include <filesystem>
#include <string>

namespace SagaProduct
{

struct SagaLocalPresenceLockReportRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::filesystem::path lockTargetPath;
    std::filesystem::path reportPath;
};

struct SagaLocalReviewAuditReportRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::filesystem::path reviewTargetPath;
    std::string comment = "Inspect project metadata";
    std::filesystem::path reportPath;
};

struct SagaLocalRolePermissionReportRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::string roleName;
    std::string permissionName;
    std::filesystem::path reportPath;
};

struct SagaLocalProjectSliceReportRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::string sliceName;
    std::filesystem::path sliceTargetPath;
    std::filesystem::path reportPath;
};

struct SagaLocalApprovalGateReportRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::string roleName;
    std::filesystem::path gateTargetPath;
    std::string approvalState = "approved-local-evaluation";
    std::filesystem::path reportPath;
};

struct SagaLocalCollaborationMetadataReportResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    std::string status;
    std::string error;
};

[[nodiscard]] SagaLocalCollaborationMetadataReportResult
WriteLocalPresenceLockReport(
    const SagaLocalPresenceLockReportRequest& request);

[[nodiscard]] SagaLocalCollaborationMetadataReportResult
WriteLocalReviewAuditReport(
    const SagaLocalReviewAuditReportRequest& request);

[[nodiscard]] SagaLocalCollaborationMetadataReportResult
WriteLocalRolePermissionReport(
    const SagaLocalRolePermissionReportRequest& request);

[[nodiscard]] SagaLocalCollaborationMetadataReportResult
WriteLocalProjectSliceReport(
    const SagaLocalProjectSliceReportRequest& request);

[[nodiscard]] SagaLocalCollaborationMetadataReportResult
WriteLocalApprovalGateReport(
    const SagaLocalApprovalGateReportRequest& request);

} // namespace SagaProduct
