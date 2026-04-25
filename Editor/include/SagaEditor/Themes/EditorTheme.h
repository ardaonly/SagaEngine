/// @file EditorTheme.h
/// @brief Full theme descriptor: color palette + font descriptor + stylesheet.

#pragma once

#include "SagaEditor/Themes/ColorPalette.h"
#include <string>

namespace SagaEditor
{

// ─── Font Descriptor ──────────────────────────────────────────────────────────

/// Minimal font specification independent of Qt.
struct FontDescriptor
{
    std::string family = "Segoe UI";    ///< Font family name.
    int pointSize = 10;                 ///< Point size (Qt units).
    bool bold = false;                  ///< Bold weight flag.
    bool italic = false;                ///< Italic style flag.
};

// ─── Editor Theme ─────────────────────────────────────────────────────────────

/// A complete visual theme. ThemeRegistry stores and applies these at runtime;
/// the user can switch themes without restarting the editor.
///
/// The stylesheet is the authoritative styling source for the UI layer.
/// ColorPalette is the semantic token layer used by viewport overlays, gizmos,
/// and any code that needs to query colors programmatically (e.g. graph nodes).
struct EditorTheme
{
    std::string name;           ///< Unique theme name (used as registry key).
    std::string description;    ///< One-line human-readable summary.
    ColorPalette palette;       ///< Semantic color tokens.
    FontDescriptor uiFont;      ///< Primary interface font.
    FontDescriptor codeFont;    ///< Monospace font for console and script editors.
    std::string stylesheet;     ///< Full stylesheet applied to UI.
    bool builtin = false;       ///< True for the five engine-shipped themes.
};

} // namespace SagaEditor
