/// @file EditorProfile.h
/// @brief Workflow profile descriptor for editor shell composition.

#pragma once

#include "SagaEditor/Commands/ShortcutManager.h"

#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Shortcut Binding ────────────────────────────────────────────────────────

struct EditorShortcutBinding
{
    KeyChord    chord;
    std::string commandId;
    std::string displayText;

    [[nodiscard]] bool operator==(const EditorShortcutBinding& other) const noexcept
    {
        return chord == other.chord &&
               commandId == other.commandId &&
               displayText == other.displayText;
    }
};

// ─── Editor Profile ──────────────────────────────────────────────────────────

/// Describes the editor workflow layer. Profiles may change shell layout,
/// visible tools, panel topology, and shortcut mapping; they must not alter
/// core selection, gizmo, asset, command, or runtime semantics.
struct EditorProfile
{
    std::string id;
    std::string displayName;
    std::string description;

    std::string layoutPresetId;
    std::string shortcutMapId;

    std::vector<std::string> defaultPanels;
    std::vector<std::string> defaultToolbarCommands;
    std::vector<std::string> visibleToolCommands;
    std::vector<EditorShortcutBinding> shortcutBindings;

    bool showMenuBar = true;
    bool showStatusBar = true;
    bool showMainToolbar = true;

    bool exposesGraphEditor = true;
    bool exposesProfiler = true;
    bool exposesConsole = true;
    bool exposesAssetBrowser = true;

    [[nodiscard]] bool operator==(const EditorProfile& other) const noexcept;
    [[nodiscard]] bool operator!=(const EditorProfile& other) const noexcept
    {
        return !(*this == other);
    }
};

[[nodiscard]] EditorProfile MakeBasicProfile();
[[nodiscard]] EditorProfile MakeNodeEditorProfile();
[[nodiscard]] EditorProfile MakeStandardPipelineProfile();
[[nodiscard]] EditorProfile MakeAdvancedPipelineProfile();
[[nodiscard]] EditorProfile MakeCustomProfile();

[[nodiscard]] std::string DefaultEditorProfileId();
[[nodiscard]] std::string ResolveEditorProfileId(const std::string& id);

} // namespace SagaEditor
