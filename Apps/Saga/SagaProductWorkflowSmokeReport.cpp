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

[[nodiscard]] std::string ShellQuote(const fs::path& path)
{
    std::string quoted = "'";
    for (const char ch : path.string())
    {
        if (ch == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

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

[[nodiscard]] nlohmann::json WorkflowStep(std::string id,
                                          std::string status,
                                          std::string command,
                                          const fs::path& expectedReportPath,
                                          std::vector<nlohmann::json> diagnostics = {})
{
    nlohmann::json step;
    step["id"] = std::move(id);
    step["status"] = std::move(status);
    step["command"] = std::move(command);
    step["expectedReportPath"] =
        expectedReportPath.empty() ? "" : expectedReportPath.string();
    step["diagnostics"] = std::move(diagnostics);
    return step;
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
        "Workflow commands are references and are not executed by this smoke report.",
        "SagaEditor owns inspection and future editing views.",
        "Visual Blocks evidence remains CLI-only and is not Visual Blocks editor UI.",
        "Package preflight is a known limitation and is not package or distribution readiness.",
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
    };
}

[[nodiscard]] nlohmann::json ReportReference(const nlohmann::json& step)
{
    nlohmann::json reference;
    reference["stepId"] = step["id"];
    reference["status"] = step["status"];
    reference["path"] = step["expectedReportPath"];
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
    const fs::path serverReport =
        tempRoot / "starter_arena_server_smoke.json";
    const fs::path serverDiagnostics =
        tempRoot / "starter_arena_server_diagnostics";

    std::vector<nlohmann::json> steps;
    steps.push_back(WorkflowStep(
        "project_validation",
        ReferenceStatus(validationReport),
        "nix-shell --run \"Tools/SagaProjectKit/sagaproject validate --project " +
            ShellQuote(projectManifest) + " --out " +
            ShellQuote(validationReport) + "\"",
        validationReport));
    steps.push_back(WorkflowStep(
        "editor_inspection",
        ReferenceStatus(editorReport),
        "build/RelWithDebInfo-0.0.9/bin/SagaEditor --inspect-project " +
            ShellQuote(projectManifest) + " --profile " + request.profile +
            " --editor-shell-report " + ShellQuote(editorReport),
        editorReport));
    steps.push_back(WorkflowStep(
        "runtime_smoke",
        ReferenceStatus(runtimeReport),
        "build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project " +
            ShellQuote(projectManifest) +
            " --starter-arena-smoke --smoke-report-out " +
            ShellQuote(runtimeReport) +
            " --smoke-frames 30 --fixed-dt 0.016",
        runtimeReport));
    steps.push_back(WorkflowStep(
        "sagascript_analyze_compile",
        ReferenceStatus(sagaScriptRoot / "Manifests" / "script_artifacts.json"),
        "Tools/SagaScript/sagascript analyze --source " +
            ShellQuote(projectRoot / "Scripts") + " --out " +
            ShellQuote(sagaScriptRoot) +
            " && Tools/SagaScript/sagascript compile --source " +
            ShellQuote(projectRoot / "Scripts") + " --out " +
            ShellQuote(sagaScriptRoot / "Manifests") +
            " --artifacts-out " +
            ShellQuote(sagaScriptRoot / "Artifacts" / "Scripts") +
            " --project-root " + ShellQuote(projectRoot) +
            " --assembly-name StarterArenaScripts --diagnostics " +
            ShellQuote(sagaScriptRoot / "sagascript_diagnostics.json") +
            " --json",
        sagaScriptRoot / "Manifests" / "script_artifacts.json"));
    steps.push_back(WorkflowStep(
        "visual_blocks_cli_chain",
        ReferenceStatus(sagaScriptRoot / "visual_blocks_projection_v1.json"),
        "compatibility-profile -> project-blocks -> plan-block-edit -> apply-block-edit -> analyze -> compile",
        sagaScriptRoot / "visual_blocks_projection_v1.json",
        { Diagnostic("Saga.Workflow.VisualBlocksCliOnly",
                     "Visual Blocks evidence is CLI-only and is not editor UI.") }));
    steps.push_back(WorkflowStep(
        "server_authority_smoke",
        ReferenceStatus(serverReport),
        "build/RelWithDebInfo-0.0.9/bin/MultiplayerSandboxHeadless --project " +
            ShellQuote(projectManifest) +
            " --starter-arena-server-smoke --report-out " +
            ShellQuote(serverReport) + " --diagnostics-out " +
            ShellQuote(serverDiagnostics) + " --ticks 1 --fixed-dt 1.0",
        serverReport));
    steps.push_back(WorkflowStep(
        "package_preflight",
        "blocked",
        "scripts/package-linux-saga",
        {},
        { Diagnostic("Saga.Workflow.PackagePreflightLimitation",
                     "Package preflight is not package or distribution readiness.") }));
    steps.push_back(WorkflowStep(
        "known_limitations",
        "available",
        "",
        {},
        KnownLimitations()));

    std::vector<nlohmann::json> commands;
    std::vector<nlohmann::json> reportReferences;
    for (const nlohmann::json& step : steps)
    {
        commands.push_back(nlohmann::json{
            { "stepId", step["id"] },
            { "command", step["command"] },
            { "status", step["status"] },
        });
        reportReferences.push_back(ReportReference(step));
    }

    bool hasFailedProject = !project.ok;
    bool hasLimitations = true;
    for (const nlohmann::json& step : steps)
    {
        const std::string status = step.value("status", "");
        if (status == "missing_report" || status == "blocked")
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
    report["schemaVersion"] = 1;
    report["tool"] = "Saga";
    report["command"] = "workflow-smoke";
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
    report["workflowSteps"] = std::move(steps);
    report["commands"] = std::move(commands);
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
