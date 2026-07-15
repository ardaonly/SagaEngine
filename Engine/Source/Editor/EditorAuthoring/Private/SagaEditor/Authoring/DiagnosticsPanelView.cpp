#include "SagaEditor/Authoring/DiagnosticsPanelView.h"

#include "SagaEditor/Authoring/ScriptArtifactIndex.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[nodiscard]] std::string StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] DiagnosticsPanelSeverity SeverityFromString(
    const std::string& severity)
{
    if (severity == "Critical" || severity == "Error" ||
        severity == "Security")
    {
        return DiagnosticsPanelSeverity::Critical;
    }
    if (severity == "Warning")
    {
        return DiagnosticsPanelSeverity::Warning;
    }
    return DiagnosticsPanelSeverity::Info;
}

void AddEntry(DiagnosticsPanelView& view, DiagnosticsPanelEntryView entry)
{
    if (entry.severity == DiagnosticsPanelSeverity::Critical)
    {
        view.critical.entries.push_back(std::move(entry));
    }
    else if (entry.severity == DiagnosticsPanelSeverity::Warning)
    {
        view.warning.entries.push_back(std::move(entry));
    }
    else
    {
        view.info.entries.push_back(std::move(entry));
    }
}

void AddAuthoringDiagnostic(DiagnosticsPanelView& view,
                            const AuthoringDiagnostic& diagnostic,
                            const std::string& source)
{
    AddEntry(view, DiagnosticsPanelEntryView{
        DiagnosticsPanelSeverity::Warning,
        diagnostic.code,
        diagnostic.message,
        diagnostic.path,
        source,
    });
}

[[nodiscard]] Json ReadJsonFile(const fs::path& path,
                                DiagnosticsPanelView& view)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        AddEntry(view, DiagnosticsPanelEntryView{
            DiagnosticsPanelSeverity::Warning,
            "Editor.DiagnosticsPanel.SummaryMissing",
            "diagnostics_summary.json is missing",
            path,
            "diagnostics_summary",
        });
        return Json();
    }
    try
    {
        Json json;
        input >> json;
        return json;
    }
    catch (const Json::exception& error)
    {
        AddEntry(view, DiagnosticsPanelEntryView{
            DiagnosticsPanelSeverity::Critical,
            "Editor.DiagnosticsPanel.SummaryInvalid",
            error.what(),
            path,
            "diagnostics_summary",
        });
        return Json();
    }
}

void LoadSummaryDiagnostics(const Json& json,
                            const fs::path& path,
                            DiagnosticsPanelView& view)
{
    const auto diagnosticsIt = json.find("diagnostics");
    if (diagnosticsIt != json.end() && diagnosticsIt->is_array())
    {
        for (const Json& diagnostic : *diagnosticsIt)
        {
            if (!diagnostic.is_object())
            {
                continue;
            }
            const std::string diagnosticPath =
                StringField(diagnostic, "sourceFile").empty()
                    ? path.string()
                    : StringField(diagnostic, "sourceFile");
            AddEntry(view, DiagnosticsPanelEntryView{
                SeverityFromString(StringField(diagnostic, "severity")),
                StringField(diagnostic, "code"),
                StringField(diagnostic, "message"),
                diagnosticPath,
                "diagnostics_summary",
            });
        }
    }

    const auto criticalIt = json.find("criticalDiagnostics");
    if (criticalIt != json.end() && criticalIt->is_array())
    {
        for (const Json& diagnostic : *criticalIt)
        {
            if (!diagnostic.is_object())
            {
                continue;
            }
            AddEntry(view, DiagnosticsPanelEntryView{
                DiagnosticsPanelSeverity::Critical,
                StringField(diagnostic, "code"),
                StringField(diagnostic, "message"),
                StringField(diagnostic, "sourceFile").empty()
                    ? path.string()
                    : StringField(diagnostic, "sourceFile"),
                "diagnostics_summary",
            });
        }
    }

    const auto warningIt = json.find("warningDiagnostics");
    if (warningIt != json.end() && warningIt->is_array())
    {
        for (const Json& diagnostic : *warningIt)
        {
            if (!diagnostic.is_object())
            {
                continue;
            }
            AddEntry(view, DiagnosticsPanelEntryView{
                DiagnosticsPanelSeverity::Warning,
                StringField(diagnostic, "code"),
                StringField(diagnostic, "message"),
                StringField(diagnostic, "sourceFile").empty()
                    ? path.string()
                    : StringField(diagnostic, "sourceFile"),
                "diagnostics_summary",
            });
        }
    }
}

} // namespace

const char* ToString(const DiagnosticsPanelSeverity severity) noexcept
{
    switch (severity)
    {
    case DiagnosticsPanelSeverity::Critical: return "Critical";
    case DiagnosticsPanelSeverity::Warning:  return "Warning";
    case DiagnosticsPanelSeverity::Info:     return "Info";
    }
    return "Info";
}

DiagnosticsPanelView LoadDiagnosticsPanelView(
    const ProjectReadinessView& project)
{
    return LoadDiagnosticsPanelView(
        LoadProjectBrowserWorkflowView(project.manifestPath),
        LoadScriptBehaviorInspectorViews(project));
}

DiagnosticsPanelView LoadDiagnosticsPanelView(
    const ProjectBrowserWorkflowView& projectBrowser,
    const ScriptBehaviorInspectorLoadResult& inspector)
{
    DiagnosticsPanelView view;
    view.critical.severity = DiagnosticsPanelSeverity::Critical;
    view.warning.severity = DiagnosticsPanelSeverity::Warning;
    view.info.severity = DiagnosticsPanelSeverity::Info;
    view.refresh.recomputedFromFiles = true;
    view.refresh.writesFiles = false;
    view.refresh.status = "RefreshedReadOnly";

    view.diagnosticsSummaryPath =
        (projectBrowser.generatedReportsPath.empty()
             ? projectBrowser.projectRoot / "Build" / "Reports"
             : projectBrowser.generatedReportsPath) /
        "diagnostics_summary.json";

    for (const AuthoringDiagnostic& diagnostic : projectBrowser.diagnostics)
    {
        AddAuthoringDiagnostic(view, diagnostic, "project_browser");
    }
    for (const AuthoringDiagnostic& diagnostic : inspector.diagnostics)
    {
        AddAuthoringDiagnostic(view, diagnostic, "behavior_inspector");
    }

    const Json summary = ReadJsonFile(view.diagnosticsSummaryPath, view);
    if (summary.is_object())
    {
        LoadSummaryDiagnostics(summary, view.diagnosticsSummaryPath, view);
    }

    view.ok = view.critical.entries.empty() && view.warning.entries.empty();
    return view;
}

} // namespace SagaEditor::Authoring
