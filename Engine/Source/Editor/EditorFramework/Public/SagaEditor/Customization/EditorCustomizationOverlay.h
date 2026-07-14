/// @file EditorCustomizationOverlay.h
/// @brief Safe user customization overlay applied to compiled editor composition artifacts.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor
{

using EditorCustomizationOverlayVersion = uint32_t;

inline constexpr EditorCustomizationOverlayVersion kCurrentEditorCustomizationOverlayVersion = 1;

/// User override for one panel placement and visibility in a workspace.
struct LayoutCustomizationOverride
{
    std::string workspaceId;
    std::string panelId;
    std::string placement;
    bool visible = true;
};

/// User override for a single action shortcut.
struct ShortcutCustomizationOverride
{
    std::string actionId;
    std::string chord;
};

/// User override for panel visibility independent of placement.
struct VisibilityCustomizationOverride
{
    std::string panelId;
    bool visible = true;
};

/// User override for hiding safe toolbar actions.
struct ToolbarCustomizationOverride
{
    std::string toolbarId;
    std::vector<std::string> hiddenActionIds;
};

/// Safe customization overlay. It cannot define new editor structure.
struct EditorCustomizationOverlay
{
    EditorCustomizationOverlayVersion schemaVersion = 0;
    std::string baseCompositionId;
    std::string baseArtifactHash;
    std::string overlayId;
    std::vector<LayoutCustomizationOverride> layoutOverrides;
    std::vector<ShortcutCustomizationOverride> shortcutOverrides;
    std::vector<VisibilityCustomizationOverride> visibilityOverrides;
    std::vector<ToolbarCustomizationOverride> toolbarOverrides;
};

} // namespace SagaEditor
