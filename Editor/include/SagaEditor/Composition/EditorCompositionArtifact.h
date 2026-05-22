/// @file EditorCompositionArtifact.h
/// @brief Data model for compiled SDE editor composition artifacts.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor
{

using EditorCompositionArtifactVersion = uint32_t;

inline constexpr EditorCompositionArtifactVersion kCurrentEditorCompositionArtifactVersion = 1;

/// Stable metadata emitted by the SDE editor composition artifact writer.
struct EditorCompositionMetadata
{
    std::string displayName;
    std::string description;
};

/// Compiled panel declaration available to the editor shell.
struct PanelDefinition
{
    std::string id;
    std::string displayName;
    std::string kind;
    bool defaultVisible = true;
    bool internalOnly = false;
    std::vector<std::string> allowedProfiles;
};

/// Compiled command/action declaration available to menus, toolbars, and shortcuts.
struct ActionDefinition
{
    std::string id;
    std::string displayName;
    std::string category;
    bool safeInProduct = true;
    bool internalOnly = false;
};

/// Menu item reference emitted by the composition artifact.
struct MenuItemDefinition
{
    std::string kind;
    std::string actionId;
    std::string menuId;
};

/// Compiled menu declaration.
struct MenuDefinition
{
    std::string id;
    std::string displayName;
    std::vector<MenuItemDefinition> items;
};

/// Compiled toolbar declaration.
struct ToolbarDefinition
{
    std::string id;
    std::string displayName;
    std::vector<std::string> actions;
};

/// Compiled shortcut declaration.
struct ShortcutBindingDefinition
{
    std::string id;
    std::string actionId;
    std::string chord;
};

/// Compiled workspace layout declaration.
struct WorkspaceLayoutDefinition
{
    std::string id;
    std::string displayName;
};

/// Compiled workspace profile declaration.
struct WorkspaceProfileDefinition
{
    std::string id;
    std::string displayName;
    std::string layoutId;
    std::vector<std::string> visiblePanels;
};

/// Compiled editor mode declaration.
struct EditorModeDefinition
{
    std::string id;
    std::string displayName;
    std::string workspaceId;
    std::vector<std::string> requiredPanels;
};

/// Immutable compiled artifact produced from saga.editor SDE source definitions.
struct EditorCompositionArtifact
{
    std::string artifactKind;
    EditorCompositionArtifactVersion artifactVersion = 0;
    std::string schemaPackage;
    uint32_t schemaPackageVersion = 0;
    std::string compositionId;
    std::string sourceMapRef;
    EditorCompositionMetadata metadata;
    std::vector<PanelDefinition> panels;
    std::vector<ActionDefinition> actions;
    std::vector<MenuDefinition> menus;
    std::vector<ToolbarDefinition> toolbars;
    std::vector<ShortcutBindingDefinition> shortcuts;
    std::vector<WorkspaceLayoutDefinition> workspaceLayouts;
    std::vector<WorkspaceProfileDefinition> workspaces;
    std::vector<EditorModeDefinition> editorModes;
};

} // namespace SagaEditor
