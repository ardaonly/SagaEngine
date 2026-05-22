/// @file ResolvedEditorCompositionSnapshot.h
/// @brief Immutable editor composition snapshot consumed by SagaEditor.

#pragma once

#include "SagaEditor/Composition/EditorCompositionArtifact.h"
#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"

#include <string>
#include <vector>

namespace SagaEditor
{

/// Resolved panel state after applying safe user customization overlays.
struct ResolvedPanelDefinition
{
    PanelDefinition definition;
    bool visible = true;
    std::string placement;
};

/// Resolved toolbar state after applying safe toolbar overrides.
struct ResolvedToolbarDefinition
{
    ToolbarDefinition definition;
    std::vector<std::string> visibleActionIds;
};

/// Final immutable editor structure consumed by the editor shell.
struct ResolvedEditorCompositionSnapshot
{
    std::string compositionId;
    std::string artifactHash;
    std::vector<std::string> appliedOverlayIds;
    std::vector<ResolvedPanelDefinition> panels;
    std::vector<ActionDefinition> actions;
    std::vector<MenuDefinition> menus;
    std::vector<ResolvedToolbarDefinition> toolbars;
    std::vector<ShortcutBindingDefinition> shortcuts;
    std::vector<WorkspaceProfileDefinition> workspaces;
    std::vector<EditorModeDefinition> editorModes;
    std::vector<EditorCompositionDiagnostic> diagnostics;
    bool isUsable = false;
};

} // namespace SagaEditor
