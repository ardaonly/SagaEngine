/// @file DockTab.h
/// @brief Descriptor for a single docked or floating panel tab.

#pragma once

#include <string>

namespace SagaEditor
{

class IPanel;

// ─── Dock Tab State ───────────────────────────────────────────────────────────

/// Placement hint used when a tab has no saved layout position.
enum class DockSlot : uint8_t
{
    Left,       ///< Dock to the left region.
    Right,      ///< Dock to the right region.
    Top,        ///< Dock to the top region.
    Bottom,     ///< Dock to the bottom region.
    Center,     ///< Place in the central viewport region.
    Floating,   ///< Open as a free-floating window.
};

// ─── Dock Tab ─────────────────────────────────────────────────────────────────

/// Associates a panel with its placement state inside the dock workspace.
/// The workspace serializes these descriptors into the layout file.
struct DockTab
{
    IPanel*     panel       = nullptr;         ///< Non-owning; owned by EditorShell.
    DockSlot    defaultSlot = DockSlot::Center; ///< Placement when no saved layout exists.
    bool        pinned      = false;            ///< Pinned tabs survive layout reset.
    float       splitRatio  = 0.25f;            ///< Fraction of the target region to occupy.

    /// Return the display title from the associated panel, or empty if panel is null.
    [[nodiscard]] std::string GetTitle() const
    {
        return panel ? panel->GetTitle() : std::string{};
    }
};

} // namespace SagaEditor
