/// @file DensityProfile.cpp
/// @brief Implementation of the persona density spectrum.

#include "SagaEditor/Persona/DensityProfile.h"

namespace SagaEditor
{

// ─── Identity ─────────────────────────────────────────────────────────────────

const char* DensityStepId(DensityStep step) noexcept
{
    switch (step)
    {
        case DensityStep::Cosy:        return "saga.density.cosy";
        case DensityStep::Comfortable: return "saga.density.comfortable";
        case DensityStep::Compact:     return "saga.density.compact";
        case DensityStep::Dense:       return "saga.density.dense";
        case DensityStep::Ultra:       return "saga.density.ultra";
        case DensityStep::Custom:      return "saga.density.custom";
    }
    return "saga.density.unknown";
}

DensityStep DensityStepFromId(const std::string& id) noexcept
{
    if (id == "saga.density.cosy")        return DensityStep::Cosy;
    if (id == "saga.density.comfortable") return DensityStep::Comfortable;
    if (id == "saga.density.compact")     return DensityStep::Compact;
    if (id == "saga.density.dense")       return DensityStep::Dense;
    if (id == "saga.density.ultra")       return DensityStep::Ultra;
    if (id == "saga.density.custom")      return DensityStep::Custom;
    return DensityStep::Comfortable;
}

// ─── Equality ─────────────────────────────────────────────────────────────────

bool DensityProfile::operator==(const DensityProfile& o) const noexcept
{
    return step              == o.step
        && displayName       == o.displayName
        && basePadding       == o.basePadding
        && sectionGap        == o.sectionGap
        && widgetGap         == o.widgetGap
        && fontBodyPt        == o.fontBodyPt
        && fontHeaderPt      == o.fontHeaderPt
        && fontMonoPt        == o.fontMonoPt
        && hitBoxMinPx       == o.hitBoxMinPx
        && rowHeightPx       == o.rowHeightPx
        && gizmoLinePx       == o.gizmoLinePx
        && showShortcutHints == o.showShortcutHints
        && showStatusLabels  == o.showStatusLabels;
}

// ─── Factories ────────────────────────────────────────────────────────────────

DensityProfile MakeCosyDensity()
{
    DensityProfile p;
    p.step              = DensityStep::Cosy;
    p.displayName       = "Cosy";
    p.basePadding       = 16.0f;
    p.sectionGap        = 24.0f;
    p.widgetGap         = 12.0f;
    p.fontBodyPt        = 14.0f;
    p.fontHeaderPt      = 18.0f;
    p.fontMonoPt        = 13.0f;
    p.hitBoxMinPx       = 40.0f;
    p.rowHeightPx       = 36.0f;
    p.gizmoLinePx       = 3.0f;
    p.showShortcutHints = false; // beginners do not need accelerator clutter.
    p.showStatusLabels  = true;
    return p;
}

DensityProfile MakeComfortableDensity()
{
    DensityProfile p;
    p.step              = DensityStep::Comfortable;
    p.displayName       = "Comfortable";
    p.basePadding       = 8.0f;
    p.sectionGap        = 12.0f;
    p.widgetGap         = 4.0f;
    p.fontBodyPt        = 11.0f;
    p.fontHeaderPt      = 13.0f;
    p.fontMonoPt        = 11.0f;
    p.hitBoxMinPx       = 24.0f;
    p.rowHeightPx       = 22.0f;
    p.gizmoLinePx       = 1.5f;
    p.showShortcutHints = true;
    p.showStatusLabels  = true;
    return p;
}

DensityProfile MakeCompactDensity()
{
    DensityProfile p;
    p.step              = DensityStep::Compact;
    p.displayName       = "Compact";
    p.basePadding       = 6.0f;
    p.sectionGap        = 10.0f;
    p.widgetGap         = 3.0f;
    p.fontBodyPt        = 10.0f;
    p.fontHeaderPt      = 12.0f;
    p.fontMonoPt        = 10.0f;
    p.hitBoxMinPx       = 22.0f;
    p.rowHeightPx       = 20.0f;
    p.gizmoLinePx       = 1.25f;
    p.showShortcutHints = true;
    p.showStatusLabels  = false;
    return p;
}

DensityProfile MakeDenseDensity()
{
    DensityProfile p;
    p.step              = DensityStep::Dense;
    p.displayName       = "Dense";
    p.basePadding       = 4.0f;
    p.sectionGap        = 8.0f;
    p.widgetGap         = 2.0f;
    p.fontBodyPt        = 9.0f;
    p.fontHeaderPt      = 11.0f;
    p.fontMonoPt        = 9.0f;
    p.hitBoxMinPx       = 18.0f;
    p.rowHeightPx       = 18.0f;
    p.gizmoLinePx       = 1.0f;
    p.showShortcutHints = true;
    p.showStatusLabels  = false;
    return p;
}

DensityProfile MakeUltraDensity()
{
    DensityProfile p;
    p.step              = DensityStep::Ultra;
    p.displayName       = "Ultra";
    p.basePadding       = 2.0f;
    p.sectionGap        = 6.0f;
    p.widgetGap         = 1.0f;
    p.fontBodyPt        = 8.0f;
    p.fontHeaderPt      = 10.0f;
    p.fontMonoPt        = 8.0f;
    p.hitBoxMinPx       = 16.0f;
    p.rowHeightPx       = 16.0f;
    p.gizmoLinePx       = 1.0f;
    p.showShortcutHints = false; // expert mode: hints are noise.
    p.showStatusLabels  = false;
    return p;
}

DensityProfile MakeDensityProfile(DensityStep step)
{
    switch (step)
    {
        case DensityStep::Cosy:        return MakeCosyDensity();
        case DensityStep::Comfortable: return MakeComfortableDensity();
        case DensityStep::Compact:     return MakeCompactDensity();
        case DensityStep::Dense:       return MakeDenseDensity();
        case DensityStep::Ultra:       return MakeUltraDensity();
        case DensityStep::Custom:      break;
    }
    // Custom defaults to Comfortable; persona authors overwrite fields
    // after construction.
    DensityProfile p = MakeComfortableDensity();
    p.step        = DensityStep::Custom;
    p.displayName = "Custom";
    return p;
}

} // namespace SagaEditor
