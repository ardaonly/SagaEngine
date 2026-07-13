/// @file EditorProjectInspection.cpp
/// @brief Implements the Editor-owned schema-2 project inspection report.

#include "SagaEditor/Host/EditorProjectInspection.h"

#include "SagaEditor/Authoring/EditorShellCustomizationReport.h"
#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"
#include "SagaEditor/Authoring/TechnicalPreviewProjectView.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <filesystem>
#include <fstream>
#include <vector>

namespace SagaEditor
{
namespace
{

namespace fs = std::filesystem;

[[nodiscard]] QJsonValue JsonString(const std::string& value)
{
    return QString::fromStdString(value);
}

[[nodiscard]] QJsonValue PathJson(const fs::path& path)
{
    return path.empty() ? QJsonValue(QJsonValue::Null) :
        JsonString(path.string());
}

[[nodiscard]] std::string PathStatus(const fs::path& path)
{
    return !path.empty() && fs::exists(path) ? "report_available" : "not_run";
}

[[nodiscard]] QJsonArray DiagnosticsJson(
    const std::vector<Authoring::AuthoringDiagnostic>& diagnostics)
{
    QJsonArray output;
    for (const auto& diagnostic : diagnostics)
    {
        output.push_back(QJsonObject{
            {"code", JsonString(diagnostic.code)},
            {"message", JsonString(diagnostic.message)},
            {"path", JsonString(diagnostic.path)},
        });
    }
    return output;
}

void AppendDiagnostics(
    std::vector<Authoring::AuthoringDiagnostic>& diagnostics,
    const std::vector<std::string>& messages)
{
    for (const std::string& message : messages)
    {
        diagnostics.push_back(Authoring::AuthoringDiagnostic{
            "Editor.Customization.UnknownProfile", message, ""});
    }
}

[[nodiscard]] QJsonArray PathReferencesJson(
    const std::vector<Authoring::TechnicalPreviewPathReference>& references)
{
    QJsonArray output;
    for (const auto& reference : references)
    {
        output.push_back(QJsonObject{
            {"id", JsonString(reference.id)},
            {"path", JsonString(reference.path.string())},
            {"exists", fs::exists(reference.path)},
        });
    }
    return output;
}

[[nodiscard]] QJsonArray BrowserSectionsJson(
    const std::vector<Authoring::ProjectBrowserSectionView>& sections)
{
    QJsonArray output;
    for (const auto& section : sections)
    {
        QJsonArray links;
        for (const auto& link : section.artifactLinks)
        {
            links.push_back(QJsonObject{
                {"id", JsonString(link.id)},
                {"kind", JsonString(link.kind)},
                {"path", JsonString(link.path.string())},
                {"exists", link.exists},
                {"openable", link.openable},
            });
        }
        output.push_back(QJsonObject{
            {"name", JsonString(section.name)},
            {"path", JsonString(section.path.string())},
            {"exists", section.exists},
            {"status", JsonString(section.status)},
            {"openableArtifactPath",
             JsonString(section.openableArtifactPath.string())},
            {"artifactLinks", links},
            {"diagnostics", DiagnosticsJson(section.diagnostics)},
        });
    }
    return output;
}

[[nodiscard]] QJsonObject TypedAction(
    const char* id,
    const char* label,
    const char* kind,
    const char* owner,
    const char* availability,
    const fs::path& reportPath = {},
    const fs::path& diagnosticsPath = {})
{
    return QJsonObject{
        {"id", id},
        {"label", label},
        {"kind", kind},
        {"owner", owner},
        {"availability", availability},
        {"status", JsonString(PathStatus(reportPath))},
        {"reportPath", PathJson(reportPath)},
        {"diagnosticsPath", PathJson(diagnosticsPath)},
    };
}

[[nodiscard]] QJsonArray WorkflowActionsJson()
{
    const fs::path temporary = fs::temp_directory_path();
    const fs::path scriptRoot = temporary / "starter_arena_sagascript";
    QJsonArray output;
    output.push_back(TypedAction(
        "validate-project", "Validate project metadata", "project-validation",
        "sagaproject", "available",
        temporary / "starter_arena_validate.json"));
    output.push_back(TypedAction(
        "runtime-smoke", "Run bounded runtime smoke", "runtime-smoke",
        "SagaRuntime", "bounded",
        temporary / "starter_arena_runtime_smoke.json"));
    output.push_back(TypedAction(
        "sagascript-analyze", "Analyze SagaScript source", "script-analysis",
        "sagascript", "available", scriptRoot / "analysis_report.json",
        scriptRoot / "sagascript_diagnostics.json"));
    output.push_back(TypedAction(
        "sagascript-compile", "Compile SagaScript source", "script-compile",
        "sagascript", "available",
        scriptRoot / "Manifests" / "script_artifacts.json",
        scriptRoot / "sagascript_diagnostics.json"));
    output.push_back(TypedAction(
        "visual-blocks-evidence", "Read Visual Blocks projection evidence",
        "report-inspection", "sagascript", "cli_only",
        scriptRoot / "visual_blocks_projection_v1.json",
        scriptRoot / "sagascript_diagnostics.json"));
    output.push_back(TypedAction(
        "package-status", "View package preflight status", "status-report",
        "Saga", "preflight_only"));
    return output;
}

[[nodiscard]] QJsonObject CustomizationJson(
    const Authoring::EditorShellCustomizationReport& customization)
{
    QJsonArray panels;
    for (const auto& panel : customization.visiblePanels)
    {
        panels.push_back(QJsonObject{
            {"id", JsonString(panel.id)}, {"visible", panel.visible}});
    }
    QJsonArray workflows;
    for (const auto& workflow : customization.workflowVisibility)
    {
        if (workflow.id == "server-smoke")
        {
            continue;
        }
        workflows.push_back(QJsonObject{
            {"id", JsonString(workflow.id)},
            {"visible", workflow.visible},
            {"available", workflow.available},
            {"reason", JsonString(workflow.reason)},
        });
    }
    QJsonArray diagnostics;
    for (const std::string& diagnostic : customization.diagnostics)
    {
        diagnostics.push_back(JsonString(diagnostic));
    }
    return QJsonObject{
        {"status", JsonString(customization.status)},
        {"requestedProfileId", JsonString(customization.requestedProfileId)},
        {"resolvedProfileId", JsonString(customization.resolvedProfileId)},
        {"viewPresetId", JsonString(customization.viewPresetId)},
        {"displayName", JsonString(customization.displayName)},
        {"layoutPresetId", JsonString(customization.layoutPresetId)},
        {"shortcutMapId", JsonString(customization.shortcutMapId)},
        {"personalPreferenceOnly", customization.personalPreferenceOnly},
        {"sharedProjectTruthMutable", customization.sharedProjectTruthMutable},
        {"profileSource", JsonString(customization.profileSource)},
        {"visiblePanels", panels},
        {"workflowVisibility", workflows},
        {"diagnostics", diagnostics},
    };
}

[[nodiscard]] QJsonArray LimitationsJson()
{
    return QJsonArray{
        "Project inspection is a read-only Editor report, not full Editor readiness.",
        "Generic Runtime project execution remains unsupported.",
        "Visual Blocks evidence remains CLI-only.",
        "Typed actions describe bounded owners and are not executed by inspection.",
        "Package status remains preflight evidence and is not release readiness.",
        "No dedicated-server executable or server launch action is available.",
    };
}

} // namespace

EditorProjectInspectionResult InspectEditorProject(
    const EditorProjectInspectionRequest& request)
{
    EditorProjectInspectionResult result;
    result.reportPath = request.reportPath;
    if (request.projectPath.empty() || request.reportPath.empty())
    {
        result.error = "Project inspection requires project and report paths.";
        return result;
    }

    const Authoring::TechnicalPreviewProjectView project =
        Authoring::LoadTechnicalPreviewProjectView(request.projectPath);
    const Authoring::ProjectBrowserWorkflowView browser =
        Authoring::LoadProjectBrowserWorkflowView(request.projectPath);
    const Authoring::EditorShellCustomizationReport customization =
        Authoring::BuildEditorShellCustomizationReport(
            request.requestedProfileId);

    result.status = project.ok ?
        (browser.diagnostics.empty() ? "Passed" : "PassedWithLimitations") :
        "Failed";
    std::vector<Authoring::AuthoringDiagnostic> diagnostics =
        browser.diagnostics.empty() ? project.diagnostics : browser.diagnostics;
    AppendDiagnostics(diagnostics, customization.diagnostics);

    const QJsonObject report{
        {"schemaVersion", 2},
        {"tool", "SagaEditor"},
        {"operation", "inspect-project"},
        {"status", JsonString(result.status)},
        {"verified", false},
        {"project", QJsonObject{
            {"projectId", JsonString(project.projectId)},
            {"displayName", JsonString(project.displayName)},
            {"projectRoot", JsonString(project.projectRoot.string())},
            {"manifestPath", JsonString(project.manifestPath.string())},
            {"diagnosticsPath", PathJson(project.diagnosticsPath)},
            {"generatedReportsPath", PathJson(project.generatedReportsPath)},
            {"scriptFolders", PathReferencesJson(project.scriptFolders)},
            {"launchProfiles", PathReferencesJson(project.launchProfiles)},
            {"packageProfiles", PathReferencesJson(project.packageProfiles)},
        }},
        {"editorReadModel", QJsonObject{
            {"technicalPreviewProjectOk", project.ok},
            {"projectBrowserOk", browser.ok},
            {"sections", BrowserSectionsJson(browser.sections)},
        }},
        {"actions", WorkflowActionsJson()},
        {"customization", CustomizationJson(customization)},
        {"diagnostics", DiagnosticsJson(diagnostics)},
        {"limitations", LimitationsJson()},
    };

    std::error_code error;
    if (!request.reportPath.parent_path().empty())
    {
        fs::create_directories(request.reportPath.parent_path(), error);
    }
    if (error)
    {
        result.error = "Unable to create inspection report directory: " +
            error.message();
        return result;
    }
    std::ofstream output(request.reportPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open())
    {
        result.error = "Unable to write Editor inspection report: " +
            request.reportPath.string();
        return result;
    }
    const QByteArray bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    output.write(bytes.constData(), bytes.size());
    result.ok = project.ok;
    return result;
}

} // namespace SagaEditor
