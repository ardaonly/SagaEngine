/// @file DensityProfile.h
/// @brief Spacing, font, and hit-box scale that drives every persona's feel.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Density Step ─────────────────────────────────────────────────────────────

/// Five-step density spectrum the editor commits to shipping. Every
/// other surface (theme, block style, panel set, toolbar) tunes its
/// own metrics against these tokens, so increasing density consistently
/// shrinks the whole UI rather than only one widget.
enum class DensityStep : std::uint8_t
{
    Cosy,         ///< Kid-friendly, large hit boxes; pairs with BlockyBeginner.
    Comfortable,  ///< Default for first-time professional users.
    Compact,      ///< Default for ProDense's middle option.
    Dense,        ///< Pro-tools default, matches Unreal-style multi-panel layouts.
    Ultra,        ///< Expert: smallest fonts, tightest padding, hidden labels.
    Custom,       ///< User-authored profile loaded from `<workspace>/Personas/`.
};

/// Stable lowercase id for `step`. Used by the JSON serializer; do not
/// rename. Returns `"saga.density.unknown"` for invalid enumerators.
[[nodiscard]] const char* DensityStepId(DensityStep step) noexcept;

/// Inverse of `DensityStepId`. Returns `DensityStep::Comfortable` when
/// the id is unrecognised — the editor never refuses to start because
/// of a stale persona file; it falls back to a sensible default.
[[nodiscard]] DensityStep DensityStepFromId(const std::string& id) noexcept;

// ─── Density Profile ──────────────────────────────────────────────────────────

/// Numeric tokens every density step exposes. Pixel values are
/// authored against a 1.0 device-pixel-ratio screen; the Qt UI backend
/// scales them by the active monitor's DPR before producing widget
/// sizes. The struct stays POD so serialisers and tests can value-compare
/// it without tripping over hidden state.
struct DensityProfile
{
    DensityStep step = DensityStep::Comfortable; ///< Identifier for diagnostics.
    std::string displayName;                     ///< User-visible label.

    // Spacing & padding (px).
    float       basePadding   = 8.0f;  ///< Inner padding inside every panel widget.
    float       sectionGap    = 12.0f; ///< Gap between major sections.
    float       widgetGap     = 4.0f;  ///< Gap between adjacent inline widgets.

    // Typography (pt at DPR=1).
    float       fontBodyPt    = 11.0f; ///< Base body font.
    float       fontHeaderPt  = 13.0f; ///< Section header font.
    float       fontMonoPt    = 11.0f; ///< Monospaced (console / inspector path).

    // Touch-friendliness.
    float       hitBoxMinPx   = 24.0f; ///< Smallest acceptable button hit-box.
    float       rowHeightPx   = 22.0f; ///< Tree / list row height.
    float       gizmoLinePx   = 1.5f;  ///< Gizmo overlay line width.

    // Visibility.
    bool        showShortcutHints = true; ///< Show keyboard hint glyphs in menus.
    bool        showStatusLabels  = true; ///< Show text labels under toolbar icons.

    // ─── Equality ─────────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const DensityProfile& other) const noexcept;
    [[nodiscard]] bool operator!=(const DensityProfile& other) const noexcept
    {
        return !(*this == other);
    }
};

// ─── Built-in Profiles ────────────────────────────────────────────────────────

/// Factory helpers for the five engine-shipped density steps.
/// Calling these produces a fully populated profile regardless of the
/// host's locale or Qt version — the constants are baked in so tests
/// can rely on them.
[[nodiscard]] DensityProfile MakeCosyDensity();
[[nodiscard]] DensityProfile MakeComfortableDensity();
[[nodiscard]] DensityProfile MakeCompactDensity();
[[nodiscard]] DensityProfile MakeDenseDensity();
[[nodiscard]] DensityProfile MakeUltraDensity();

/// Materialise a density profile from a `DensityStep`. Returns the
/// matching `Make*` factory output; returns `MakeComfortableDensity()`
/// for `DensityStep::Custom` (the persona system fills custom values
/// in by overwriting fields after this call).
[[nodiscard]] DensityProfile MakeDensityProfile(DensityStep step);

} // namespace SagaEditor
