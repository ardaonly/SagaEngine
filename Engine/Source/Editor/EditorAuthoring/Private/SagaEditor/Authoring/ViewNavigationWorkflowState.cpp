#include "SagaEditor/Authoring/ViewNavigationWorkflowState.h"

#include <filesystem>

namespace SagaEditor::Authoring
{
namespace
{

namespace fs = std::filesystem;

[[nodiscard]] ViewNavigationViewState MakeState(
    const EditorWorkflowViewKind kind,
    const ViewNavigationSelectionState& selection)
{
    ViewNavigationViewState state;
    state.viewKind = kind;
    state.selection = selection;
    state.canMutateSource = false;
    state.canApplyPatch = false;
    state.canShowDiagnostics = true;
    state.disclosesOpaqueDeferredUnsupported = true;
    state.canShowSourceLinks =
        kind == EditorWorkflowViewKind::Pro ||
        kind == EditorWorkflowViewKind::CSharpSource;
    return state;
}

void AddDiagnostic(ViewNavigationWorkflowState& state,
                   const std::string& code,
                   const std::string& message,
                   const fs::path& path)
{
    state.diagnostics.push_back(AuthoringDiagnostic{
        code,
        message,
        path.string(),
    });
}

[[nodiscard]] const ScriptBlockProjectionNodeView* FindSelectedNode(
    const ScriptBehaviorInspectorLoadResult& inspector,
    const ViewNavigationSelectionState& selection)
{
    for (const ScriptBehaviorInspectorView& behavior : inspector.behaviors)
    {
        if (behavior.behaviorId != selection.behaviorId)
        {
            continue;
        }
        for (const ScriptBlockProjectionNodeView& node : behavior.nodes)
        {
            if (node.nodeId == selection.nodeId)
            {
                return &node;
            }
        }
    }
    return nullptr;
}

} // namespace

const char* ToString(const EditorWorkflowViewKind viewKind) noexcept
{
    switch (viewKind)
    {
    case EditorWorkflowViewKind::Simple:       return "Simple";
    case EditorWorkflowViewKind::Pro:          return "Pro";
    case EditorWorkflowViewKind::CSharpSource: return "CSharpSource";
    }
    return "Simple";
}

ViewNavigationWorkflowState BuildViewNavigationWorkflowState(
    const TechnicalPreviewProjectView& project,
    const ScriptBehaviorInspectorLoadResult& inspector,
    const ViewNavigationSelectionState& selection)
{
    ViewNavigationWorkflowState state;
    state.simple = MakeState(EditorWorkflowViewKind::Simple, selection);
    state.pro = MakeState(EditorWorkflowViewKind::Pro, selection);
    state.csharpSource =
        MakeState(EditorWorkflowViewKind::CSharpSource, selection);

    if (!project.ok)
    {
        state.diagnostics = project.diagnostics;
        return state;
    }

    const ScriptSourceLinkView& link = selection.sourceLink;
    if (link.sourceFile.empty())
    {
        AddDiagnostic(
            state,
            "Editor.ViewNavigation.SourceLinkMissing",
            "selected behavior or node does not have a source link",
            selection.artifactPath);
    }
    else if (!fs::exists(link.sourceFile))
    {
        AddDiagnostic(
            state,
            "Editor.ViewNavigation.SourceFileMissing",
            "selected source link points to a missing source file",
            link.sourceFile);
    }
    else if (link.freshness == ScriptArtifactStatus::Stale)
    {
        AddDiagnostic(
            state,
            "Editor.ViewNavigation.SourceLinkStale",
            "selected source link is stale",
            link.sourceFile);
    }

    const ScriptBlockProjectionNodeView* node =
        FindSelectedNode(inspector, selection);
    if (node != nullptr)
    {
        if (!node->opaqueReason.empty() ||
            node->projectionCompatibility == "Opaque" ||
            node->projectionCompatibility == "Unsupported")
        {
            AddDiagnostic(
                state,
                "Editor.ViewNavigation.UnsupportedRegionDisclosed",
                "selected region is opaque or unsupported and remains read-only",
                node->sourceLink.sourceFile);
        }
        if (node->capability == "Deferred" ||
            node->projectionCompatibility == "Deferred")
        {
            AddDiagnostic(
                state,
                "Editor.ViewNavigation.DeferredRegionDisclosed",
                "selected deferred region is visible but is not runtime proof",
                node->sourceLink.sourceFile);
        }
        if (node->runtimeProofState == "RuntimeBackedWithEvidenceMissing")
        {
            AddDiagnostic(
                state,
                "Editor.ViewNavigation.RuntimeEvidenceMissing",
                "selected runtime-backed region is missing runtime evidence",
                node->sourceLink.sourceFile);
        }
    }

    state.ok = inspector.ok && state.diagnostics.empty();
    return state;
}

} // namespace SagaEditor::Authoring
