#pragma once

#include "SagaEditor/Authoring/ScriptBehaviorInspectorView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

enum class EditorWorkflowViewKind
{
    Simple,
    Pro,
    CSharpSource,
};

struct ViewNavigationSelectionState
{
    std::string behaviorId;
    std::string nodeId;
    std::filesystem::path artifactPath;
    ScriptSourceLinkView sourceLink;
};

struct ViewNavigationViewState
{
    EditorWorkflowViewKind viewKind = EditorWorkflowViewKind::Simple;
    bool canMutateSource = false;
    bool canApplyPatch = false;
    bool canShowSourceLinks = false;
    bool canShowDiagnostics = true;
    bool disclosesOpaqueDeferredUnsupported = true;
    ViewNavigationSelectionState selection;
};

struct ViewNavigationWorkflowState
{
    bool ok = false;
    ViewNavigationViewState simple;
    ViewNavigationViewState pro;
    ViewNavigationViewState csharpSource;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] const char* ToString(EditorWorkflowViewKind viewKind) noexcept;

[[nodiscard]] ViewNavigationWorkflowState BuildViewNavigationWorkflowState(
    const ProjectReadinessView& project,
    const ScriptBehaviorInspectorLoadResult& inspector,
    const ViewNavigationSelectionState& selection);

} // namespace SagaEditor::Authoring
