/// @file ColorPalette.h
/// @brief Named color tokens used by EditorTheme — the single source of color truth.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Color Structure ──────────────────────────────────────────────────────────

/// Simple RGBA color representation.
struct Color
{
    uint8_t r = 0;     ///< Red channel (0-255).
    uint8_t g = 0;     ///< Green channel (0-255).
    uint8_t b = 0;     ///< Blue channel (0-255).
    uint8_t a = 255;   ///< Alpha channel (0-255, 255=opaque).
};

// ─── Color Palette ────────────────────────────────────────────────────────────

/// All semantic color slots an EditorTheme must define.
/// UI code refers to these tokens; themes supply the actual Color values.
/// Adding a new token here automatically surfaces it in ThemeRegistry's
/// validation pass so incomplete themes fail fast at load time.
struct ColorPalette
{
    // Backgrounds
    Color windowBackground;        ///< Main window / panel background.
    Color panelBackground;         ///< Individual panel widget background.
    Color dockTabBackground;       ///< Inactive dock tab background.
    Color dockTabActiveBackground; ///< Active / focused dock tab background.
    Color menuBackground;          ///< Menu and context-menu background.
    Color toolbarBackground;       ///< Toolbar strip background.
    Color statusBarBackground;     ///< Bottom status bar background.

    // Foregrounds
    Color textPrimary;             ///< Standard label and body text.
    Color textSecondary;           ///< Subdued / hint text.
    Color textDisabled;            ///< Disabled widget text.
    Color textOnAccent;            ///< Text drawn on top of accent-coloured surfaces.

    // Accent & interactive
    Color accent;                  ///< Primary brand / selection accent color.
    Color accentHover;             ///< Accent when hovered.
    Color accentPressed;           ///< Accent when pressed.
    Color selectionHighlight;      ///< Item selection highlight in lists/trees.

    // Borders & separators
    Color border;                  ///< Panel and widget borders.
    Color separator;               ///< Thin divider lines inside panels.

    // Status
    Color statusInfo;              ///< Info / log message indicator.
    Color statusWarning;           ///< Warning message indicator.
    Color statusError;             ///< Error message indicator.
    Color statusSuccess;           ///< Success / OK message indicator.

    // Viewport gizmos (world viewport overlay colors)
    Color gizmoAxisX;              ///< X-axis gizmo color (typically red).
    Color gizmoAxisY;              ///< Y-axis gizmo color (typically green).
    Color gizmoAxisZ;              ///< Z-axis gizmo color (typically blue).
    Color gizmoSelected;           ///< Selected-entity gizmo highlight.
};

// ─── Built-in Palettes ────────────────────────────────────────────────────────

/// Factory helpers for the five engine-shipped color palettes.
ColorPalette MakeDarkPalette();
ColorPalette MakeLightPalette();
ColorPalette MakeNordPalette();
ColorPalette MakeSolarizedDarkPalette();
ColorPalette MakeMidnightPalette();

} // namespace SagaEditor
