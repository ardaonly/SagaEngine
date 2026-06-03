/// @file main.cpp
/// @brief SagaEditor platform entry point.
///
/// Qt handles the WinMain shim on Windows via qt_add_executable(WIN32) in
/// SagaTargets.cmake — no separate WinMain wrapper is needed.

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Authoring/EditorShellCustomizationReport.h"
#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"
#include "SagaEditor/Authoring/TechnicalPreviewProjectView.h"
#include "SagaEditor/UI/Qt/QtUIFactory.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitStartupFailure = 1;
constexpr int kExitBadArguments = 2;
constexpr int kExitInspectionFailure = 3;

namespace fs = std::filesystem;

[[nodiscard]] bool HasValue(int index, int argc) noexcept
{
    return index + 1 < argc;
}

[[nodiscard]] bool ParseInt(const char* text, int& out) noexcept
{
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0)
    {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

[[nodiscard]] QJsonValue JsonString(const std::string& value)
{
    return QString::fromStdString(value);
}

[[nodiscard]] std::string PathStatus(const fs::path& path)
{
    return !path.empty() && fs::exists(path) ? "report_available" : "not_run";
}

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

[[nodiscard]] QJsonValue PathJson(const fs::path& path)
{
    return path.empty() ? QJsonValue(QJsonValue::Null) : JsonString(path.string());
}

[[nodiscard]] QJsonArray DiagnosticsJson(
    const std::vector<SagaEditor::Authoring::AuthoringDiagnostic>& diagnostics)
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
    std::vector<SagaEditor::Authoring::AuthoringDiagnostic>& diagnostics,
    const std::vector<std::string>& messages)
{
    for (const std::string& message : messages)
    {
        diagnostics.push_back(SagaEditor::Authoring::AuthoringDiagnostic{
            "Editor.Customization.UnknownProfile",
            message,
            "" });
    }
}

[[nodiscard]] QJsonArray PathReferencesJson(
    const std::vector<SagaEditor::Authoring::TechnicalPreviewPathReference>& refs)
{
    QJsonArray output;
    for (const auto& ref : refs)
    {
        output.push_back(QJsonObject{
            {"id", JsonString(ref.id)},
            {"path", JsonString(ref.path.string())},
            {"exists", fs::exists(ref.path)},
        });
    }
    return output;
}

[[nodiscard]] QJsonArray ProjectBrowserSectionsJson(
    const std::vector<SagaEditor::Authoring::ProjectBrowserSectionView>& sections)
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

[[nodiscard]] QJsonObject WorkflowAction(
    const std::string& id,
    const std::string& label,
    const std::string& command,
    const fs::path& reportPath,
    const fs::path& diagnosticsPath = {})
{
    return QJsonObject{
        {"id", JsonString(id)},
        {"label", JsonString(label)},
        {"proofSource", "cli"},
        {"command", JsonString(command)},
        {"status", JsonString(PathStatus(reportPath))},
        {"reportPath", PathJson(reportPath)},
        {"diagnosticsPath", PathJson(diagnosticsPath)},
        {"exitStatus", QJsonValue(QJsonValue::Null)},
    };
}

[[nodiscard]] QJsonArray WorkflowActionsJson(const fs::path& projectManifest)
{
    const fs::path starterRuntimeReport =
        fs::temp_directory_path() / "starter_arena_runtime_smoke.json";
    const fs::path starterScriptRoot =
        fs::temp_directory_path() / "starter_arena_sagascript";
    const fs::path starterServerReport =
        fs::temp_directory_path() / "starter_arena_server_smoke.json";
    const fs::path starterServerDiagnostics =
        fs::temp_directory_path() / "starter_arena_server_diagnostics";
    const fs::path validationReport =
        fs::temp_directory_path() / "starter_arena_validate.json";

    QJsonArray output;
    output.push_back(WorkflowAction(
        "validate-project",
        "Validate project metadata",
        "nix-shell --run \"Tools/SagaProjectKit/sagaproject validate --project " +
            ShellQuote(projectManifest) + " --out " +
            ShellQuote(validationReport) + "\"",
        validationReport));
    output.push_back(WorkflowAction(
        "runtime-smoke",
        "Run runtime smoke",
        "build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project " +
            ShellQuote(projectManifest) +
            " --starter-arena-smoke --smoke-report-out " +
            ShellQuote(starterRuntimeReport) +
            " --smoke-frames 30 --fixed-dt 0.016",
        starterRuntimeReport));
    output.push_back(WorkflowAction(
        "sagascript-analyze",
        "Run SagaScript analyze",
        "Tools/SagaScript/sagascript analyze --source samples/StarterArena/Scripts --out " +
            ShellQuote(starterScriptRoot),
        starterScriptRoot / "analysis_report.json",
        starterScriptRoot / "sagascript_diagnostics.json"));
    output.push_back(WorkflowAction(
        "sagascript-compile",
        "Run SagaScript compile",
        "Tools/SagaScript/sagascript compile --source samples/StarterArena/Scripts --out " +
            ShellQuote(starterScriptRoot / "Manifests") +
            " --artifacts-out " +
            ShellQuote(starterScriptRoot / "Artifacts" / "Scripts") +
            " --project-root samples/StarterArena --assembly-name StarterArenaScripts --diagnostics " +
            ShellQuote(starterScriptRoot / "sagascript_diagnostics.json") +
            " --json",
        starterScriptRoot / "Manifests" / "script_artifacts.json",
        starterScriptRoot / "sagascript_diagnostics.json"));
    output.push_back(WorkflowAction(
        "visual-blocks-cli-chain",
        "Read CLI-only Visual Blocks evidence",
        "compatibility-profile -> project-blocks -> plan-block-edit -> apply-block-edit -> analyze -> compile",
        starterScriptRoot / "visual_blocks_projection_v1.json",
        starterScriptRoot / "sagascript_diagnostics.json"));
    output.push_back(WorkflowAction(
        "server-smoke",
        "Run server-authority smoke",
        "build/RelWithDebInfo-0.0.9/bin/MultiplayerSandboxHeadless --project " +
            ShellQuote(projectManifest) +
            " --starter-arena-server-smoke --report-out " +
            ShellQuote(starterServerReport) + " --diagnostics-out " +
            ShellQuote(starterServerDiagnostics) + " --ticks 1 --fixed-dt 1.0",
        starterServerReport,
        starterServerDiagnostics));
    QJsonObject packagePreflight = WorkflowAction(
        "package-preflight",
        "Run package preflight",
        "scripts/package-linux-saga",
        fs::path{});
    packagePreflight["status"] = "preflight_known_limitation";
    output.push_back(packagePreflight);
    return output;
}

[[nodiscard]] QJsonArray KnownLimitationsJson()
{
    QJsonArray output;
    output.push_back(
        "Phase 22/23 is an editor shell inspection/report workflow, not a full editor UI.");
    output.push_back(
        "No drag/drop editing or in-place C# editing is implemented by this mode.");
    output.push_back(
        "Visual Blocks evidence remains CLI-only and is not Visual Blocks editor UI.");
    output.push_back(
        "Workflow actions are command/report references and are not executed by this inspection mode.");
    output.push_back(
        "Package preflight remains preflight-only and is not distribution readiness.");
    output.push_back("No phase is marked Verified by this report.");
    return output;
}

[[nodiscard]] QJsonObject CustomizationCapabilitiesJson(
    const SagaEditor::Authoring::EditorShellCustomizationCapabilities& capabilities)
{
    return QJsonObject{
        {"canViewProject", capabilities.canViewProject},
        {"canViewDiagnostics", capabilities.canViewDiagnostics},
        {"canViewScriptProjection", capabilities.canViewScriptProjection},
        {"canRunWorkflowCommandReference",
         capabilities.canRunWorkflowCommandReference},
        {"canMutateProjectManifest", capabilities.canMutateProjectManifest},
        {"canEditCSharpInPlace", capabilities.canEditCSharpInPlace},
        {"canUseVisualBlocksEditorUi", capabilities.canUseVisualBlocksEditorUi},
        {"canConfigureCollaborationServer",
         capabilities.canConfigureCollaborationServer},
        {"canBuildDistribution", capabilities.canBuildDistribution},
    };
}

[[nodiscard]] QJsonObject CustomizationJson(
    const SagaEditor::Authoring::EditorShellCustomizationReport& customization)
{
    QJsonArray panels;
    for (const auto& panel : customization.visiblePanels)
    {
        panels.push_back(QJsonObject{
            {"id", JsonString(panel.id)},
            {"visible", panel.visible},
        });
    }

    QJsonArray workflows;
    for (const auto& workflow : customization.workflowVisibility)
    {
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
        {"capabilities", CustomizationCapabilitiesJson(customization.capabilities)},
        {"diagnostics", diagnostics},
    };
}

[[nodiscard]] int RunProjectInspection(
    const fs::path& inspectProjectPath,
    const fs::path& reportPath,
    const std::string& requestedProfileId)
{
    const SagaEditor::Authoring::TechnicalPreviewProjectView project =
        SagaEditor::Authoring::LoadTechnicalPreviewProjectView(
            inspectProjectPath);
    const SagaEditor::Authoring::ProjectBrowserWorkflowView browser =
        SagaEditor::Authoring::LoadProjectBrowserWorkflowView(
            inspectProjectPath);
    const SagaEditor::Authoring::EditorShellCustomizationReport customization =
        SagaEditor::Authoring::BuildEditorShellCustomizationReport(
            requestedProfileId);

    const std::string status = project.ok
        ? (browser.diagnostics.empty() ? "Passed" : "PassedWithLimitations")
        : "Failed";
    std::vector<SagaEditor::Authoring::AuthoringDiagnostic> diagnostics =
        browser.diagnostics.empty() ? project.diagnostics : browser.diagnostics;
    AppendDiagnostics(diagnostics, customization.diagnostics);

    QJsonObject report{
        {"schemaVersion", 1},
        {"tool", "SagaEditor"},
        {"command", "inspect-project"},
        {"status", JsonString(status)},
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
            {"sections", ProjectBrowserSectionsJson(browser.sections)},
        }},
        {"workflowActions", WorkflowActionsJson(project.manifestPath)},
        {"customization", CustomizationJson(customization)},
        {"diagnostics", DiagnosticsJson(diagnostics)},
        {"knownLimitations", KnownLimitationsJson()},
    };

    std::error_code ec;
    fs::create_directories(reportPath.parent_path(), ec);
    std::ofstream output(reportPath, std::ios::trunc | std::ios::binary);
    if (!output.is_open())
    {
        std::cerr << "SagaEditor: unable to write editor shell report: "
                  << reportPath << '\n';
        return kExitInspectionFailure;
    }
    const QByteArray bytes =
        QJsonDocument(report).toJson(QJsonDocument::Indented);
    output.write(bytes.constData(), bytes.size());
    std::cout << "SagaEditor: editor shell report=" << reportPath << '\n';
    return project.ok ? 0 : kExitInspectionFailure;
}

[[nodiscard]] bool BuildEditorConfig(int argc,
                                     char* argv[],
                                     SagaEditor::EditorAppConfig& cfg,
                                     std::string& inspectProjectPath,
                                     std::string& editorShellReportPath)
{
    cfg.argc = argc;
    cfg.argv = argv;
    cfg.windowTitle = "SagaEditor";
    cfg.maximized = true;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--workspace")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --workspace requires a path\n";
                return false;
            }
            cfg.workspacePath = argv[++i];
        }
        else if (arg == "--profile")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --profile requires a profile id\n";
                return false;
            }
            cfg.initialProfileId = argv[++i];
        }
        else if (arg == "--composition-manifest")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --composition-manifest requires a path\n";
                return false;
            }
            cfg.composition.manifestPath = argv[++i];
        }
        else if (arg == "--composition-overlay")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --composition-overlay requires a path\n";
                return false;
            }
            cfg.composition.overlayPaths.emplace_back(argv[++i]);
        }
        else if (arg == "--require-composition")
        {
            cfg.composition.requireValidComposition = true;
        }
        else if (arg == "--smoke")
        {
            cfg.smoke = true;
            cfg.showOnInit = false;
        }
        else if (arg == "--smoke-timeout-ms")
        {
            if (!HasValue(i, argc) || !ParseInt(argv[++i], cfg.smokeTimeoutMs))
            {
                std::cerr << "SagaEditor: --smoke-timeout-ms requires a positive integer\n";
                return false;
            }
        }
        else if (arg == "--no-show")
        {
            cfg.showOnInit = false;
        }
        else if (arg == "--windowed")
        {
            cfg.maximized = false;
        }
        else if (arg == "--width")
        {
            if (!HasValue(i, argc) || !ParseInt(argv[++i], cfg.windowWidth))
            {
                std::cerr << "SagaEditor: --width requires a positive integer\n";
                return false;
            }
        }
        else if (arg == "--height")
        {
            if (!HasValue(i, argc) || !ParseInt(argv[++i], cfg.windowHeight))
            {
                std::cerr << "SagaEditor: --height requires a positive integer\n";
                return false;
            }
        }
        else if (arg == "--inspect-project")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --inspect-project requires a path\n";
                return false;
            }
            inspectProjectPath = argv[++i];
        }
        else if (arg == "--editor-shell-report")
        {
            if (!HasValue(i, argc))
            {
                std::cerr << "SagaEditor: --editor-shell-report requires a path\n";
                return false;
            }
            editorShellReportPath = argv[++i];
        }
        else
        {
            std::cerr << "SagaEditor: unknown argument '" << arg << "'\n";
            return false;
        }
    }

    if (inspectProjectPath.empty() != editorShellReportPath.empty())
    {
        std::cerr << "SagaEditor: --inspect-project and --editor-shell-report must be used together\n";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    SagaEditor::QtUIFactory factory;

    SagaEditor::EditorAppConfig cfg;
    std::string inspectProjectPath;
    std::string editorShellReportPath;
    if (!BuildEditorConfig(argc, argv, cfg, inspectProjectPath, editorShellReportPath))
    {
        return kExitBadArguments;
    }

    if (!inspectProjectPath.empty())
    {
        return RunProjectInspection(
            inspectProjectPath,
            editorShellReportPath,
            cfg.initialProfileId);
    }

    SagaEditor::EditorApp app;

    if (!app.Init(cfg, factory))
    {
        std::cerr << "SagaEditor: startup failed\n";
        return kExitStartupFailure;
    }

    const int exitCode = cfg.smoke ? app.RunSmoke(cfg.smokeTimeoutMs) : app.Run();
    app.Shutdown();
    return exitCode;
}
