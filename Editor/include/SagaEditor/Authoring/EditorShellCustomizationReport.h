#pragma once

#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct EditorShellPanelVisibility
{
    std::string id;
    bool visible = true;
};

struct EditorShellWorkflowVisibility
{
    std::string id;
    bool visible = false;
    bool available = true;
    std::string reason;
};

struct EditorShellCustomizationCapabilities
{
    bool canViewProject = true;
    bool canViewDiagnostics = true;
    bool canViewScriptProjection = true;
    bool canRunWorkflowCommandReference = true;
    bool canMutateProjectManifest = false;
    bool canEditCSharpInPlace = false;
    bool canUseVisualBlocksEditorUi = false;
    bool canConfigureCollaborationServer = false;
    bool canBuildDistribution = false;
};

struct EditorShellCustomizationReport
{
    std::string status;
    std::string requestedProfileId;
    std::string resolvedProfileId;
    std::string viewPresetId;
    std::string displayName;
    std::string layoutPresetId;
    std::string shortcutMapId;
    bool personalPreferenceOnly = true;
    bool sharedProjectTruthMutable = false;
    std::string profileSource = "builtin-report-metadata";
    std::vector<EditorShellPanelVisibility> visiblePanels;
    std::vector<EditorShellWorkflowVisibility> workflowVisibility;
    EditorShellCustomizationCapabilities capabilities;
    std::vector<std::string> diagnostics;
};

[[nodiscard]] EditorShellCustomizationReport BuildEditorShellCustomizationReport(
    const std::string& requestedProfileId);

} // namespace SagaEditor::Authoring
