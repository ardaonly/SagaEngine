/// @file ColorPalette.cpp
/// @brief Factory implementations for the five built-in color palettes.

#include "SagaEditor/Themes/ColorPalette.h"

namespace SagaEditor
{

// ─── Dark Palette ─────────────────────────────────────────────────────────────

ColorPalette MakeDarkPalette()
{
    ColorPalette p;
    p.windowBackground        = QColor("#1e1e1e");
    p.panelBackground         = QColor("#252526");
    p.dockTabBackground       = QColor("#2d2d2d");
    p.dockTabActiveBackground = QColor("#1e1e1e");
    p.menuBackground          = QColor("#2d2d2d");
    p.toolbarBackground       = QColor("#2d2d2d");
    p.statusBarBackground     = QColor("#007acc");
    p.textPrimary             = QColor("#d4d4d4");
    p.textSecondary           = QColor("#858585");
    p.textDisabled            = QColor("#555555");
    p.textOnAccent            = QColor("#ffffff");
    p.accent                  = QColor("#0e639c");
    p.accentHover             = QColor("#1177bb");
    p.accentPressed           = QColor("#0a4f7e");
    p.selectionHighlight      = QColor("#264f78");
    p.border                  = QColor("#3a3a3a");
    p.separator               = QColor("#333333");
    p.statusInfo              = QColor("#75beff");
    p.statusWarning           = QColor("#ffcc02");
    p.statusError             = QColor("#f48771");
    p.statusSuccess           = QColor("#89d185");
    p.gizmoAxisX              = QColor("#e06c75");
    p.gizmoAxisY              = QColor("#98c379");
    p.gizmoAxisZ              = QColor("#61afef");
    p.gizmoSelected           = QColor("#e5c07b");
    return p;
}

// ─── Light Palette ────────────────────────────────────────────────────────────

ColorPalette MakeLightPalette()
{
    ColorPalette p;
    p.windowBackground        = QColor("#f3f3f3");
    p.panelBackground         = QColor("#ffffff");
    p.dockTabBackground       = QColor("#e8e8e8");
    p.dockTabActiveBackground = QColor("#ffffff");
    p.menuBackground          = QColor("#f0f0f0");
    p.toolbarBackground       = QColor("#e8e8e8");
    p.statusBarBackground     = QColor("#0078d4");
    p.textPrimary             = QColor("#1e1e1e");
    p.textSecondary           = QColor("#717171");
    p.textDisabled            = QColor("#aaaaaa");
    p.textOnAccent            = QColor("#ffffff");
    p.accent                  = QColor("#0078d4");
    p.accentHover             = QColor("#106ebe");
    p.accentPressed           = QColor("#005a9e");
    p.selectionHighlight      = QColor("#cce4f7");
    p.border                  = QColor("#d4d4d4");
    p.separator               = QColor("#e0e0e0");
    p.statusInfo              = QColor("#0078d4");
    p.statusWarning           = QColor("#d08700");
    p.statusError             = QColor("#d32f2f");
    p.statusSuccess           = QColor("#2e7d32");
    p.gizmoAxisX              = QColor("#c62828");
    p.gizmoAxisY              = QColor("#2e7d32");
    p.gizmoAxisZ              = QColor("#1565c0");
    p.gizmoSelected           = QColor("#f57c00");
    return p;
}

// ─── Nord Palette ─────────────────────────────────────────────────────────────

ColorPalette MakeNordPalette()
{
    ColorPalette p;
    p.windowBackground        = QColor("#2e3440");
    p.panelBackground         = QColor("#3b4252");
    p.dockTabBackground       = QColor("#3b4252");
    p.dockTabActiveBackground = QColor("#2e3440");
    p.menuBackground          = QColor("#3b4252");
    p.toolbarBackground       = QColor("#3b4252");
    p.statusBarBackground     = QColor("#5e81ac");
    p.textPrimary             = QColor("#d8dee9");
    p.textSecondary           = QColor("#81a1c1");
    p.textDisabled            = QColor("#4c566a");
    p.textOnAccent            = QColor("#eceff4");
    p.accent                  = QColor("#5e81ac");
    p.accentHover             = QColor("#81a1c1");
    p.accentPressed           = QColor("#4c6a8e");
    p.selectionHighlight      = QColor("#4c566a");
    p.border                  = QColor("#4c566a");
    p.separator               = QColor("#434c5e");
    p.statusInfo              = QColor("#88c0d0");
    p.statusWarning           = QColor("#ebcb8b");
    p.statusError             = QColor("#bf616a");
    p.statusSuccess           = QColor("#a3be8c");
    p.gizmoAxisX              = QColor("#bf616a");
    p.gizmoAxisY              = QColor("#a3be8c");
    p.gizmoAxisZ              = QColor("#81a1c1");
    p.gizmoSelected           = QColor("#ebcb8b");
    return p;
}

// ─── Solarized Dark Palette ───────────────────────────────────────────────────

ColorPalette MakeSolarizedDarkPalette()
{
    ColorPalette p;
    p.windowBackground        = QColor("#002b36");
    p.panelBackground         = QColor("#073642");
    p.dockTabBackground       = QColor("#073642");
    p.dockTabActiveBackground = QColor("#002b36");
    p.menuBackground          = QColor("#073642");
    p.toolbarBackground       = QColor("#073642");
    p.statusBarBackground     = QColor("#268bd2");
    p.textPrimary             = QColor("#839496");
    p.textSecondary           = QColor("#586e75");
    p.textDisabled            = QColor("#073642");
    p.textOnAccent            = QColor("#fdf6e3");
    p.accent                  = QColor("#268bd2");
    p.accentHover             = QColor("#2aa198");
    p.accentPressed           = QColor("#1a6fa0");
    p.selectionHighlight      = QColor("#073642");
    p.border                  = QColor("#586e75");
    p.separator               = QColor("#073642");
    p.statusInfo              = QColor("#268bd2");
    p.statusWarning           = QColor("#b58900");
    p.statusError             = QColor("#dc322f");
    p.statusSuccess           = QColor("#859900");
    p.gizmoAxisX              = QColor("#dc322f");
    p.gizmoAxisY              = QColor("#859900");
    p.gizmoAxisZ              = QColor("#268bd2");
    p.gizmoSelected           = QColor("#b58900");
    return p;
}

// ─── Midnight Palette ─────────────────────────────────────────────────────────

ColorPalette MakeMidnightPalette()
{
    ColorPalette p;
    p.windowBackground        = QColor("#000000");
    p.panelBackground         = QColor("#0a0a0a");
    p.dockTabBackground       = QColor("#111111");
    p.dockTabActiveBackground = QColor("#000000");
    p.menuBackground          = QColor("#0a0a0a");
    p.toolbarBackground       = QColor("#0a0a0a");
    p.statusBarBackground     = QColor("#1a1a2e");
    p.textPrimary             = QColor("#e0e0e0");
    p.textSecondary           = QColor("#666666");
    p.textDisabled            = QColor("#333333");
    p.textOnAccent            = QColor("#ffffff");
    p.accent                  = QColor("#6c63ff");
    p.accentHover             = QColor("#7c74ff");
    p.accentPressed           = QColor("#5a52e0");
    p.selectionHighlight      = QColor("#16213e");
    p.border                  = QColor("#222222");
    p.separator               = QColor("#1a1a1a");
    p.statusInfo              = QColor("#6c63ff");
    p.statusWarning           = QColor("#ffc107");
    p.statusError             = QColor("#ff5252");
    p.statusSuccess           = QColor("#69f0ae");
    p.gizmoAxisX              = QColor("#ff5252");
    p.gizmoAxisY              = QColor("#69f0ae");
    p.gizmoAxisZ              = QColor("#6c63ff");
    p.gizmoSelected           = QColor("#ffd740");
    return p;
}

} // namespace SagaEditor
