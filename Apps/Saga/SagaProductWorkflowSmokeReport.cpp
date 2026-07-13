/// @file SagaProductWorkflowSmokeReport.cpp
/// @brief Report-only Product Shell workflow smoke model.

#include "SagaProductWorkflowSmokeReport.h"

#include <nlohmann/json.hpp>

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

[[nodiscard]] std::string ReferenceStatus(const fs::path& expectedReportPath)
{
    if (expectedReportPath.empty())
    {
        return "not_run";
    }
    return fs::exists(expectedReportPath) ? "available" : "missing_report";
}

[[nodiscard]] nlohmann::json WorkflowAction(
    std::string id,
    std::string status,
    std::string actionKind,
    std::string owner,
    const fs::path& expectedReportPath,
    std::string availability,
    std::string unavailableReason = {},
    std::string diagnosticId = {})
{
    nlohmann::json action;
    action["id"] = std::move(id);
    action["status"] = std::move(status);
    action["actionKind"] = std::move(actionKind);
    action["owner"] = std::move(owner);
    action["expectedReportPath"] =
        expectedReportPath.empty() ? "" : expectedReportPath.string();
    action["availability"] = std::move(availability);
    if (!unavailableReason.empty())
    {
        action["unavailableReason"] = std::move(unavailableReason);
    }
    if (!diagnosticId.empty())
    {
        action["diagnosticId"] = std::move(diagnosticId);
    }
    return action;
}

[[nodiscard]] ProjectMetadata LoadProjectMetadata(const fs::path& inputPath)
{
    ProjectMetadata metadata;
    metadata.manifestPath = fs::absolute(inputPath);
    metadata.projectRoot = metadata.manifestPath.parent_path();

    if (inputPath.empty())
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.Workflow.ProjectMissing",
            "workflow smoke requires --project <.sagaproj>"));
        return metadata;
    }
    if (!fs::exists(metadata.manifestPath))
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.Workflow.ProjectManifestMissing",
            "project manifest is missing",
            metadata.manifestPath));
        return metadata;
    }

    std::ifstream input(metadata.manifestPath, std::ios::binary);
    if (!input.is_open())
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.Workflow.ProjectManifestUnreadable",
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
                "Saga.Workflow.ProjectIdMissing",
                "project manifest does not contain projectId",
                metadata.manifestPath));
        }
    }
    catch (const std::exception& ex)
    {
        metadata.diagnostics.push_back(Diagnostic(
            "Saga.Workflow.ProjectManifestParseFailed",
            ex.what(),
            metadata.manifestPath));
    }

    return metadata;
}

[[nodiscard]] std::vector<nlohmann::json> KnownLimitations()
{
    return {
        "Product Shell workflow smoke is a report-only workflow router proof, not a full dashboard UI.",
        "Typed workflow actions are status/report references and are not executed by this smoke report.",
        "SagaEditor owns inspection and future editing views.",
        "Visual Blocks evidence remains CLI-only and is not Visual Blocks editor UI.",
        "Package preflight remains bounded evidence and is not package or distribution readiness.",
        "Repository-only server evidence is not a Product Shell workflow action.",
        "Generic server execution remains unsupported and no dedicated-server executable is included.",
        "No phase is marked Verified by this report.",
    };
}

[[nodiscard]] std::vector<nlohmann::json> NonClaims()
{
    return {
        "full product dashboard",
        "full editor UI",
        "Visual Blocks editor UI",
        "package readiness",
        "distribution readiness",
        "product beta",
        "enterprise-ready workflow",
        "maximum customization",
        "generic server support",
        "dedicated-server product workflow",
    };
}

[[nodiscard]] nlohmann::json ReportReference(const nlohmann::json& action)
{
    nlohmann::json reference;
    reference["actionId"] = action["id"];
    reference["status"] = action["status"];
    reference["path"] = action["expectedReportPath"];
    return reference;
}

} // namespace

SagaProductWorkflowSmokeResult WriteProductWorkflowSmokeReport(
    const SagaProductWorkflowSmokeRequest& request)
{
    SagaProductWorkflowSmokeResult result;
    result.reportPath = request.reportPath;

    const ProjectMetadata project = LoadProjectMetadata(request.projectManifestPath);
    const fs::path projectManifest = project.manifestPath;
    const fs::path projectRoot = project.projectRoot;
    const fs::path tempRoot = fs::temp_directory_path();
    const fs::path validationReport = tempRoot / "starter_arena_validate.json";
    const fs::path editorReport =
        tempRoot / "starter_arena_editor_shell_report.json";
    const fs::path runtimeReport =
        tempRoot / "starter_arena_runtime_smoke.json";
    const fs::path sagaScriptRoot = tempRoot / "starter_arena_sagascript";
    const fs::path packagePreflightReport =
        tempRoot / "saga_linux_package_preflight_report.json";

    std::vector<nlohmann::json> actions;
    actions.push_back(WorkflowAction(
        "project_validation",
        ReferenceStatus(validationReport),
        "project-validation",
        "sagaproject",
        validationReport,
        "available"));
    actions.push_back(WorkflowAction(
        "editor_inspection",
        ReferenceStatus(editorReport),
        "editor-inspection",
        "SagaEditor",
        editorReport,
        "available"));
    actions.push_back(WorkflowAction(
        "runtime_smoke",
        ReferenceStatus(runtimeReport),
        "runtime-smoke",
        "SagaRuntime",
        runtimeReport,
        "bounded"));
    actions.push_back(WorkflowAction(
        "sagascript_analyze_compile",
        ReferenceStatus(sagaScriptRoot / "Manifests" / "script_artifacts.json"),
        "script-analysis-compile",
        "sagascript",
        sagaScriptRoot / "Manifests" / "script_artifacts.json",
        "available"));
    actions.push_back(WorkflowAction(
        "visual_blocks_cli_chain",
        ReferenceStatus(sagaScriptRoot / "visual_blocks_projection_v1.json"),
        "visual-blocks-cli-chain",
        "sagascript",
        sagaScriptRoot / "visual_blocks_projection_v1.json",
        "cli_only",
        "Visual Blocks remains a CLI-only evidence chain.",
        "Saga.Workflow.VisualBlocksCliOnly"));
    actions.push_back(WorkflowAction(
        "package_preflight",
        ReferenceStatus(packagePreflightReport),
        "package-preflight",
        "package-linux-saga",
        packagePreflightReport,
        "preflight_only",
        "Package preflight is not package or distribution readiness.",
        "Saga.Workflow.PackagePreflightOnly"));
    actions.push_back(WorkflowAction(
        "known_limitations",
        "available",
        "status-reference",
        "Saga",
        {},
        "available"));

    std::vector<nlohmann::json> reportReferences;
    for (const nlohmann::json& action : actions)
    {
        reportReferences.push_back(ReportReference(action));
    }

    bool hasFailedProject = !project.ok;
    bool hasLimitations = true;
    for (const nlohmann::json& action : actions)
    {
        const std::string status = action.value("status", "");
        if (status == "missing_report")
        {
            hasLimitations = true;
        }
        if (status == "failed")
        {
            hasFailedProject = true;
        }
    }

    result.status = hasFailedProject ? "Failed" :
        (hasLimitations ? "PassedWithLimitations" : "Passed");

    nlohmann::json report;
    report["schemaVersion"] = 2;
    report["tool"] = "Saga";
    report["action"] = "workflow-smoke";
    report["status"] = result.status;
    report["verified"] = false;
    report["project"] = {
        { "projectId", project.projectId },
        { "displayName", project.displayName },
        { "manifestPath", projectManifest.string() },
        { "projectRoot", projectRoot.string() },
    };
    report["profile"] = {
        { "requestedProfileId", request.profile.empty() ?
            "technical_preview" : request.profile },
    };
    report["workflowActions"] = std::move(actions);
    report["reportReferences"] = std::move(reportReferences);
    report["diagnostics"] = project.diagnostics;
    report["knownLimitations"] = KnownLimitations();
    report["nonClaims"] = NonClaims();

    std::error_code ec;
    fs::create_directories(request.reportPath.parent_path(), ec);
    std::ofstream output(request.reportPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open())
    {
        result.ok = false;
        result.error = "unable to write workflow report: " +
            request.reportPath.string();
        return result;
    }
    output << report.dump(2) << '\n';
    result.ok = true;
    return result;
}

} // namespace SagaProduct
