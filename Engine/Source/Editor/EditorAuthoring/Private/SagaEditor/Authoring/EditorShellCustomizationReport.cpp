#include "SagaEditor/Authoring/EditorShellCustomizationReport.h"

#include "SagaEditor/Profile/EditorProfile.h"

#include <algorithm>
#include <array>

namespace SagaEditor::Authoring
{
namespace
{

struct ProfileSelection
{
    std::string requestedProfileId;
    std::string resolvedProfileId;
    std::string viewPresetId;
    bool known = true;
};

struct WorkflowRule
{
    const char* id;
    bool visible;
    const char* reason;
};

[[nodiscard]] ProfileSelection ResolveReportProfile(
    const std::string& requestedProfileId)
{
    const std::string requested =
        requestedProfileId.empty() ? "technical_preview" : requestedProfileId;

    if (requested == "technical_preview")
    {
        return { requested, "saga.profile.basic", "technical_preview", true };
    }
    if (requested == "script_authoring")
    {
        return { requested, "saga.profile.node_editor", "script_authoring", true };
    }
    if (requested == "diagnostics")
    {
        return { requested, "saga.profile.advanced_pipeline", "diagnostics", true };
    }
    if (requested == "server_authority")
    {
        return { requested, "saga.profile.advanced_pipeline", "server_authority", true };
    }

    const std::string resolved = ResolveEditorProfileId(requested);
    if (resolved == "saga.profile.basic")
    {
        return { requested, resolved, "technical_preview", true };
    }
    if (resolved == "saga.profile.node_editor")
    {
        return { requested, resolved, "script_authoring", true };
    }
    if (resolved == "saga.profile.standard_pipeline" ||
        resolved == "saga.profile.custom")
    {
        return { requested, resolved, "technical_preview", true };
    }
    if (resolved == "saga.profile.advanced_pipeline")
    {
        return { requested, resolved, "diagnostics", true };
    }

    return { requested, "saga.profile.basic", "technical_preview", false };
}

[[nodiscard]] SagaEditor::EditorProfile MakeResolvedProfile(
    const std::string& resolvedProfileId)
{
    if (resolvedProfileId == "saga.profile.node_editor")
    {
        return MakeNodeEditorProfile();
    }
    if (resolvedProfileId == "saga.profile.standard_pipeline")
    {
        return MakeStandardPipelineProfile();
    }
    if (resolvedProfileId == "saga.profile.advanced_pipeline")
    {
        return MakeAdvancedPipelineProfile();
    }
    if (resolvedProfileId == "saga.profile.custom")
    {
        return MakeCustomProfile();
    }
    return MakeBasicProfile();
}

[[nodiscard]] std::vector<EditorShellWorkflowVisibility> WorkflowVisibilityFor(
    const std::string& viewPresetId)
{
    static constexpr std::array<WorkflowRule, 7> kTechnicalPreview{ {
        { "validate-project", true, "Project metadata reference is primary for technical preview inspection." },
        { "runtime-smoke", true, "Runtime smoke report reference is primary technical preview evidence." },
        { "sagascript-analyze", false, "SagaScript analyze reference remains available as secondary script evidence." },
        { "sagascript-compile", false, "SagaScript compile reference remains available as secondary script evidence." },
        { "visual-blocks-cli-chain", true, "Script projection is visible only as CLI evidence, not Visual Blocks editor UI." },
        { "server-smoke", true, "Server-authority smoke report reference is visible technical preview evidence." },
        { "package-preflight", true, "Package preflight reference is visible but is not distribution readiness." },
    } };

    static constexpr std::array<WorkflowRule, 7> kScriptAuthoring{ {
        { "validate-project", true, "Project metadata reference remains visible for script authoring context." },
        { "runtime-smoke", false, "Runtime smoke reference remains available but is not primary for script authoring." },
        { "sagascript-analyze", true, "SagaScript analyze reference is primary script authoring evidence." },
        { "sagascript-compile", true, "SagaScript compile reference is primary script authoring evidence." },
        { "visual-blocks-cli-chain", true, "CLI-only block projection evidence is visible without editor UI mutation." },
        { "server-smoke", false, "Server-authority smoke reference remains available but secondary." },
        { "package-preflight", false, "Package preflight reference remains available but secondary." },
    } };

    static constexpr std::array<WorkflowRule, 7> kDiagnostics{ {
        { "validate-project", true, "Project validation reference is primary diagnostics evidence." },
        { "runtime-smoke", true, "Runtime smoke report reference is primary diagnostics evidence." },
        { "sagascript-analyze", false, "SagaScript analyze reference remains available as secondary diagnostics context." },
        { "sagascript-compile", false, "SagaScript compile reference remains available as secondary diagnostics context." },
        { "visual-blocks-cli-chain", false, "CLI-only block evidence remains available but secondary." },
        { "server-smoke", true, "Server-authority smoke report reference is primary diagnostics evidence." },
        { "package-preflight", false, "Package preflight reference remains available but secondary." },
    } };

    static constexpr std::array<WorkflowRule, 7> kServerAuthority{ {
        { "validate-project", true, "Project metadata reference remains visible for server-authority context." },
        { "runtime-smoke", false, "Runtime smoke reference remains available but secondary." },
        { "sagascript-analyze", false, "SagaScript analyze reference remains available but secondary." },
        { "sagascript-compile", false, "SagaScript compile reference remains available but secondary." },
        { "visual-blocks-cli-chain", false, "CLI-only block evidence remains available but secondary." },
        { "server-smoke", true, "Server-authority smoke report reference is primary server-authority evidence." },
        { "package-preflight", false, "Package preflight reference remains available but secondary." },
    } };

    const auto* selected = &kTechnicalPreview;
    if (viewPresetId == "script_authoring")
    {
        selected = &kScriptAuthoring;
    }
    else if (viewPresetId == "diagnostics")
    {
        selected = &kDiagnostics;
    }
    else if (viewPresetId == "server_authority")
    {
        selected = &kServerAuthority;
    }

    std::vector<EditorShellWorkflowVisibility> output;
    output.reserve(selected->size());
    for (const WorkflowRule& rule : *selected)
    {
        output.push_back(
            EditorShellWorkflowVisibility{ rule.id, rule.visible, true, rule.reason });
    }
    return output;
}

} // namespace

EditorShellCustomizationReport BuildEditorShellCustomizationReport(
    const std::string& requestedProfileId)
{
    const ProfileSelection selection = ResolveReportProfile(requestedProfileId);
    const SagaEditor::EditorProfile profile =
        MakeResolvedProfile(selection.resolvedProfileId);

    EditorShellCustomizationReport report;
    report.status = selection.known ? "Ready" : "UnknownProfile";
    report.requestedProfileId = selection.requestedProfileId;
    report.resolvedProfileId = selection.resolvedProfileId;
    report.viewPresetId = selection.viewPresetId;
    report.displayName = profile.displayName;
    report.layoutPresetId = profile.layoutPresetId;
    report.shortcutMapId = profile.shortcutMapId;

    report.visiblePanels.reserve(profile.defaultPanels.size());
    for (const std::string& panelId : profile.defaultPanels)
    {
        report.visiblePanels.push_back(EditorShellPanelVisibility{ panelId, true });
    }

    report.workflowVisibility = WorkflowVisibilityFor(selection.viewPresetId);

    if (!selection.known)
    {
        report.diagnostics.push_back(
            "Unknown editor profile or view preset '" + selection.requestedProfileId +
            "'; using technical_preview report metadata.");
    }

    return report;
}

} // namespace SagaEditor::Authoring
