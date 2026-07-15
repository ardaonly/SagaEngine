/// @file SagaLocalWorkspaceTransactionReport.cpp
/// @brief Report-only local workspace transaction boundary proof.

#include "ProductIntegration/SagaLocalWorkspaceTransactionReport.h"

#include <nlohmann/json.hpp>

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
            "Saga.LocalWorkspace.ProjectMissing",
            "local workspace transaction smoke requires --project <.sagaproj>"));
        return metadata;
    }
    if (!fs::exists(metadata.manifestPath))
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalWorkspace.ProjectManifestMissing",
            "project manifest is missing",
            metadata.manifestPath));
        return metadata;
    }

    std::ifstream input(metadata.manifestPath, std::ios::binary);
    if (!input.is_open())
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalWorkspace.ProjectManifestUnreadable",
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
                "Saga.LocalWorkspace.ProjectIdMissing",
                "project manifest does not contain projectId",
                metadata.manifestPath));
        }
    }
    catch (const std::exception& ex)
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.LocalWorkspace.ProjectManifestParseFailed",
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

[[nodiscard]] std::string MakeTransactionId(const std::string& workspaceId,
                                            const std::string& projectId,
                                            const std::string& actorId,
                                            const std::string& operationKind)
{
    return "local-transaction:" + workspaceId + ":" + projectId + ":" +
        actorId + ":" + operationKind;
}

[[nodiscard]] std::vector<nlohmann::json> OperationExamples()
{
    return {
        "OpenProject",
        "InspectProject",
        "PlanScriptBlockEdit",
        "ApplyCopiedSourcePatch",
        "RunWorkflowSmokeReference",
    };
}

[[nodiscard]] std::vector<nlohmann::json> KnownLimitations()
{
    return {
        "Local workspace transaction smoke is a no-UI report-only boundary proof.",
        "The report does not write durable collaboration metadata.",
        "The report does not mutate project files, scenes, or scripts.",
        "Personal editor view/profile metadata is not shared project truth.",
        "Semantic transaction metadata is not a real-time collaboration workflow.",
        "Enterprise policy, cloud sync, CRDT/OT, and collaboration server work are deferred.",
        "No delivery stage is marked verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> NonClaims()
{
    return {
        "full multiplayer collaboration",
        "cloud workspace",
        "enterprise permissions",
        "real-time team editing",
        "CRDT/OT",
        "collaboration server",
        "full team workspace",
        "product beta",
        "distribution readiness",
    };
}

} // namespace

SagaLocalWorkspaceTransactionResult WriteLocalWorkspaceTransactionReport(
    const SagaLocalWorkspaceTransactionRequest& request)
{
    SagaLocalWorkspaceTransactionResult result;
    result.reportPath = request.reportPath;

    const ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const std::string workspaceSelector =
        OrDefault(request.workspaceSelector, "builtin:basic");
    const std::string workspaceId = WorkspaceIdForSelector(workspaceSelector);
    const std::string actorId = OrDefault(request.actorId, "local.actor");
    const std::string operationKind =
        OrDefault(request.operationKind, "InspectProject");
    const std::string projectId = project.projectId.empty() ?
        "unknown-project" : project.projectId;
    const std::string transactionStatus = project.ok ? "ready" : "failed";

    result.status = project.ok ? "Ready" : "Failed";

    nlohmann::json report;
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "local-workspace-transaction-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["workspace"] = {
        { "workspaceId", workspaceId },
        { "selector", workspaceSelector },
    };
    report["project"] = {
        { "projectId", project.projectId },
        { "displayName", project.displayName },
        { "manifestPath", project.manifestPath.string() },
        { "projectRoot", project.projectRoot.string() },
    };
    report["transaction"] = {
        { "transactionId", MakeTransactionId(workspaceId,
                                             projectId,
                                             actorId,
                                             operationKind) },
        { "workspaceId", workspaceId },
        { "projectId", project.projectId },
        { "actorId", actorId },
        { "operationKind", operationKind },
        { "targetArtifact", project.manifestPath.string() },
        { "readOnlyEvaluation", true },
        { "status", transactionStatus },
    };
    report["operationExamples"] = OperationExamples();
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = KnownLimitations();
    report["nonClaims"] = NonClaims();

    std::error_code ec;
    if (!request.reportPath.parent_path().empty())
    {
        fs::create_directories(request.reportPath.parent_path(), ec);
    }
    std::ofstream output(request.reportPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open())
    {
        result.ok = false;
        result.error = "unable to write local workspace transaction report: " +
            request.reportPath.string();
        return result;
    }
    output << report.dump(2) << '\n';
    result.ok = true;
    return result;
}

} // namespace SagaProduct
