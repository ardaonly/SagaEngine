/// @file ColorPalette.cpp
/// @brief Factory implementations for the five built-in color palettes.

#include "SagaEditor/Themes/ColorPalette.h"

namespace SagaEditor
{

// ─── Dark Palette ─────────────────────────────────────────────────────────────

ColorPalette MakeDarkPalette()
{
    ColorPalette p;
    p.windowBackground        = Color{  30,  30,  30, 255 };
    p.panelBackground         = Color{  37,  37,  38, 255 };
    p.dockTabBackground       = Color{  45,  45,  45, 255 };
    p.dockTabActiveBackground = Color{  30,  30,  30, 255 };
    p.menuBackground          = Color{  45,  45,  45, 255 };
    p.toolbarBackground       = Color{  45,  45,  45, 255 };
    p.statusBarBackground     = Color{   0, 122, 204, 255 };
    p.textPrimary             = Color{ 212, 212, 212, 255 };
    p.textSecondary           = Color{ 133, 133, 133, 255 };
    p.textDisabled            = Color{  85,  85,  85, 255 };
    p.textOnAccent            = Color{ 255, 255, 255, 255 };
    p.accent                  = Color{  14,  99, 156, 255 };
    p.accentHover             = Color{  17, 119, 187, 255 };
    p.accentPressed           = Color{  10,  79, 126, 255 };
    p.selectionHighlight      = Color{  38,  79, 120, 255 };
    p.border                  = Color{  58,  58,  58, 255 };
    p.separator               = Color{  51,  51,  51, 255 };
    p.statusInfo              = Color{ 117, 190, 255, 255 };
    p.statusWarning           = Color{ 255, 204,   2, 255 };
    p.statusError             = Color{ 244, 135, 113, 255 };
    p.statusSuccess           = Color{ 137, 209, 133, 255 };
    p.gizmoAxisX              = Color{ 224, 108, 117, 255 };
    p.gizmoAxisY              = Color{ 152, 195, 121, 255 };
    p.gizmoAxisZ              = Color{  97, 175, 239, 255 };
    p.gizmoSelected           = Color{ 229, 192, 123, 255 };
    return p;
}

// ─── Light Palette ────────────────────────────────────────────────────────────

ColorPalette MakeLightPalette()
{
    ColorPalette p;
    p.windowBackground        = Color{ 243, 243, 243, 255 };
    p.panelBackground         = Color{ 255, 255, 255, 255 };
    p.dockTabBackground       = Color{ 232, 232, 232, 255 };
    p.dockTabActiveBackground = Color{ 255, 255, 255, 255 };
    p.menuBackground          = Color{ 240, 240, 240, 255 };
    p.toolbarBackground       = Color{ 232, 232, 232, 255 };
    p.statusBarBackground     = Color{   0, 120, 212, 255 };
    p.textPrimary             = Color{  30,  30,  30, 255 };
    p.textSecondary           = Color{ 113, 113, 113, 255 };
    p.textDisabled            = Color{ 170, 170, 170, 255 };
    p.textOnAccent            = Color{ 255, 255, 255, 255 };
    p.accent                  = Color{   0, 120, 212, 255 };
    p.accentHover             = Color{  16, 110, 190, 255 };
    p.accentPressed           = Color{   0,  90, 158, 255 };
    p.selectionHighlight      = Color{ 204, 228, 247, 255 };
    p.border                  = Color{ 212, 212, 212, 255 };
    p.separator               = Color{ 224, 224, 224, 255 };
    p.statusInfo              = Color{   0, 120, 212, 255 };
    p.statusWarning           = Color{ 208, 135,   0, 255 };
    p.statusError             = Color{ 211,  47,  47, 255 };
    p.statusSuccess           = Color{  46, 125,  50, 255 };
    p.gizmoAxisX              = Color{ 198,  40,  40, 255 };
    p.gizmoAxisY              = Color{  46, 125,  50, 255 };
    p.gizmoAxisZ              = Color{  21, 101, 192, 255 };
    p.gizmoSelected           = Color{ 245, 124,   0, 255 };
    return p;
}

// ─── Nord Palette ─────────────────────────────────────────────────────────────

ColorPalette MakeNordPalette()
{
    ColorPalette p;
    p.windowBackground        = Color{  46,  52,  64, 255 };
    p.panelBackground         = Color{  59,  66,  82, 255 };
    p.dockTabBackground       = Color{  59,  66,  82, 255 };
    p.dockTabActiveBackground = Color{  46,  52,  64, 255 };
    p.menuBackground          = Color{  59,  66,  82, 255 };
    p.toolbarBackground       = Color{  59,  66,  82, 255 };
    p.statusBarBackground     = Color{  94, 129, 172, 255 };
    p.textPrimary             = Color{ 216, 222, 233, 255 };
    p.textSecondary           = Color{ 129, 161, 193, 255 };
    p.textDisabled            = Color{  76,  86, 106, 255 };
    p.textOnAccent            = Color{ 236, 239, 244, 255 };
    p.accent                  = Color{  94, 129, 172, 255 };
    p.accentHover             = Color{ 129, 161, 193, 255 };
    p.accentPressed           = Color{  76, 106, 142, 255 };
    p.selectionHighlight      = Color{  76,  86, 106, 255 };
    p.border                  = Color{  76,  86, 106, 255 };
    p.separator               = Color{  67,  76,  94, 255 };
    p.statusInfo              = Color{ 136, 192, 208, 255 };
    p.statusWarning           = Color{ 235, 203, 139, 255 };
    p.statusError             = Color{ 191,  97, 106, 255 };
    p.statusSuccess           = Color{ 163, 190, 140, 255 };
    p.gizmoAxisX              = Color{ 191,  97, 106, 255 };
    p.gizmoAxisY              = Color{ 163, 190, 140, 255 };
    p.gizmoAxisZ              = Color{ 129, 161, 193, 255 };
    p.gizmoSelected           = Color{ 235, 203, 139, 255 };
    return p;
}

// ─── Solarized Dark Palette ───────────────────────────────────────────────────

ColorPalette MakeSolarizedDarkPalette()
{
    ColorPalette p;
    p.windowBackground        = Color{   0,  43,  54, 255 };
    p.panelBackground         = Color{   7,  54,  66, 255 };
    p.dockTabBackground       = Color{   7,  54,  66, 255 };
    p.dockTabActiveBackground = Color{   0,  43,  54, 255 };
    p.menuBackground          = Color{   7,  54,  66, 255 };
    p.toolbarBackground       = Color{   7,  54,  66, 255 };
    p.statusBarBackground     = Color{  38, 139, 210, 255 };
    p.textPrimary             = Color{ 131, 148, 150, 255 };
    p.textSecondary           = Color{  88, 110, 117, 255 };
    p.textDisabled            = Color{   7,  54,  66, 255 };
    p.textOnAccent            = Color{ 253, 246, 227, 255 };
    p.accent                  = Color{  38, 139, 210, 255 };
    p.accentHover             = Color{  42, 161, 152, 255 };
    p.accentPressed           = Color{  26, 111, 160, 255 };
    p.selectionHighlight      = Color{   7,  54,  66, 255 };
    p.border                  = Color{  88, 110, 117, 255 };
    p.separator               = Color{   7,  54,  66, 255 };
    p.statusInfo              = Color{  38, 139, 210, 255 };
    p.statusWarning           = Color{ 181, 137,   0, 255 };
    p.statusError             = Color{ 220,  50,  47, 255 };
    p.statusSuccess           = Color{ 133, 153,   0, 255 };
    p.gizmoAxisX              = Color{ 220,  50,  47, 255 };
    p.gizmoAxisY              = Color{ 133, 153,   0, 255 };
    p.gizmoAxisZ              = Color{  38, 139, 210, 255 };
    p.gizmoSelected           = Color{ 181, 137,   0, 255 };
    return p;
}

// ─── Midnight Palette ─────────────────────────────────────────────────────────

ColorPalette MakeMidnightPalette()
{
    ColorPalette p;
    p.windowBackground        = Color{   0,   0,   0, 255 };
    p.panelBackground         = Color{  10,  10,  10, 255 };
    p.dockTabBackground       = Color{  17,  17,  17, 255 };
    p.dockTabActiveBackground = Color{   0,   0,   0, 255 };
    p.menuBackground          = Color{  10,  10,  10, 255 };
    p.toolbarBackground       = Color{  10,  10,  10, 255 };
    p.statusBarBackground     = Color{  26,  26,  46, 255 };
    p.textPrimary             = Color{ 224, 224, 224, 255 };
    p.textSecondary           = Color{ 102, 102, 102, 255 };
    p.textDisabled            = Color{  51,  51,  51, 255 };
    p.textOnAccent            = Color{ 255, 255, 255, 255 };
    p.accent                  = Color{ 108,  99, 255, 255 };
    p.accentHover             = Color{ 124, 116, 255, 255 };
    p.accentPressed           = Color{  90,  82, 224, 255 };
    p.selectionHighlight      = Color{  22,  33,  62, 255 };
    p.border                  = Color{  34,  34,  34, 255 };
    p.separator               = Color{  26,  26,  26, 255 };
    p.statusInfo              = Color{ 108,  99, 255, 255 };
    p.statusWarning           = Color{ 255, 193,   7, 255 };
    p.statusError             = Color{ 255,  82,  82, 255 };
    p.statusSuccess           = Color{ 105, 240, 174, 255 };
    p.gizmoAxisX              = Color{ 255,  82,  82, 255 };
    p.gizmoAxisY              = Color{ 105, 240, 174, 255 };
    p.gizmoAxisZ              = Color{ 108,  99, 255, 255 };
    p.gizmoSelected           = Color{ 255, 215,  64, 255 };
    return p;
}

} // namespace SagaEditor
