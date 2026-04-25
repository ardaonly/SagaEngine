/// @file WorkspacePreset.h
/// @brief Named workspace combining a dock layout, theme, and toolbar configuration.

#pragma once

#include <string>

namespace SagaEditor
{

// ─── Workspace Preset ─────────────────────────────────────────────────────────

/// A complete workspace identity: layout + theme + toolbar visibility.
/// Five presets ship with the engine; users may save unlimited custom presets.
///
/// Built-in presets:
///   Default   — balanced layout suitable for most workflows.
///   Creator   — wide viewport, collapsed inspector, expanded asset browser.
///   Indie     — minimal chrome, large viewport, hidden profiler and collaboration panels.
///   Studio    — full chrome, four-viewport grid, inspector, profiler.
///   Technical — dense layout, console prominent, profiler and zone-debug panels open.
struct WorkspacePreset
{
    std::string name;         ///< Unique identifier and display name.
    std::string description;  ///< One-sentence summary shown in the preset picker.
    std::string layoutPreset; ///< Name of the LayoutPreset to apply.
    std::string themeName;    ///< Name of the EditorTheme to apply.
    bool        showToolbar  = true;  ///< Show the main toolbar.
    bool        showStatusBar = true; ///< Show the bottom status bar.
    bool        builtin       = false; ///< True for the five engine-shipped presets.
};

} // namespace SagaEditor
