/// @file ShellLayout.h
/// @brief Descriptor for the top-level shell chrome: menu bar, toolbars, status bar.

#pragma once

#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Menu Item ────────────────────────────────────────────────────────────────

/// One item in the shell menu bar — maps a label to a command ID.
struct MenuItemDescriptor
{
    std::string label;      ///< Human-readable label (e.g. "Save Scene").
    std::string commandId;  ///< Command invoked when this item is clicked.
    std::string shortcut;   ///< Display hint only; binding lives in ShortcutManager.
    bool        separator = false; ///< When true, render a separator instead of a label.
};

/// One top-level menu (e.g. "File", "Edit", "View").
struct MenuDescriptor
{
    std::string                     title; ///< Menu header label.
    std::vector<MenuItemDescriptor> items; ///< Ordered list of menu entries.
};

// ─── Shell Layout Descriptor ─────────────────────────────────────────────────

/// Declarative description of the shell chrome assembled during startup.
/// Extensions and packages can contribute menus and toolbar entries before
/// the shell renders its first frame.
struct ShellLayout
{
    std::vector<MenuDescriptor> menus;          ///< Top-level menu bar entries, left to right.
    bool                        showMenuBar    = true;  ///< Toggle the entire menu bar.
    bool                        showStatusBar  = true;  ///< Toggle the bottom status bar.
    bool                        showMainToolbar = true; ///< Toggle the top toolbar row.
};

} // namespace SagaEditor
