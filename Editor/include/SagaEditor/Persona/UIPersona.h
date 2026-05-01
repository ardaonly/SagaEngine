/// @file UIPersona.h
/// @brief Top-level persona descriptor — composes theme, density, and block style.

#pragma once

#include "SagaEditor/Persona/BlockVisualStyle.h"
#include "SagaEditor/Persona/DensityProfile.h"

#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Persona Tier ─────────────────────────────────────────────────────────────

/// Position along the customisability spectrum the persona occupies.
/// Used by the welcome-screen picker to suggest a starting point and
/// by the package compatibility check to flag personas that the
/// package does not target.
enum class PersonaTier : std::uint8_t
{
    Beginner,   ///< BlockyBeginner — kid-friendly, blocks-first.
    Indie,      ///< IndieBalanced  — mid-density, hybrid blocks/pins.
    Pro,        ///< ProDense       — multi-panel, dense pro-tools layout.
    Technical,  ///< Technical      — pro defaults plus diagnostics tools.
    Custom,     ///< User-authored persona.
};

[[nodiscard]] const char*  PersonaTierId(PersonaTier tier) noexcept;
[[nodiscard]] PersonaTier  PersonaTierFromId(const std::string& id) noexcept;

// ─── UI Persona ───────────────────────────────────────────────────────────────

/// Composes everything the editor swaps when the user picks a persona
/// at startup or through the View menu. Personas are JSON-serialisable
/// so they can ship as built-ins, be exported by users, or be added
/// by extension packages.
struct UIPersona
{
    // Identity.
    std::string  id;            ///< Stable lowercase id (`saga.persona.pro`).
    std::string  displayName;   ///< User-visible label.
    std::string  description;   ///< One-sentence summary shown under the name.
    PersonaTier  tier = PersonaTier::Indie;

    // References into companion registries.
    std::string  themeId;             ///< Name in `ThemeRegistry`.
    std::string  workspacePresetId;   ///< Layout preset filename without extension.
    std::string  shortcutMapId;       ///< Reserved for the ShortcutManager profile.

    // Embedded value types.
    DensityProfile   density   = MakeComfortableDensity();
    BlockVisualStyle blockStyle = MakePinNodeStyle();

    // Default-visible panels (matches `IPanel::GetPanelId` values).
    std::vector<std::string> defaultPanels;

    // Default-pinned toolbar entries (matches command ids).
    std::vector<std::string> defaultToolbarCommands;

    // Hint flags exposed to the welcome-screen and the persona picker.
    bool         exposesGraphEditor = true;  ///< Show the visual scripting graph by default.
    bool         exposesProfiler    = true;  ///< Show the profiler panel by default.
    bool         exposesConsole     = true;  ///< Show the console panel by default.
    bool         exposesAssetBrowser = true; ///< Show the asset browser by default.
    bool         showShortcutHints   = true; ///< Mirror of density's hint visibility.

    // ─── Equality ─────────────────────────────────────────────────────────────

    [[nodiscard]] bool operator==(const UIPersona& other) const noexcept;
    [[nodiscard]] bool operator!=(const UIPersona& other) const noexcept
    {
        return !(*this == other);
    }
};

// ─── Built-in Personas ────────────────────────────────────────────────────────

/// Kid-friendly, blocks-first authoring. Inspired by Scratch.
[[nodiscard]] UIPersona MakeBlockyBeginnerPersona();

/// Mid-density hybrid persona for indie developers.
[[nodiscard]] UIPersona MakeIndieBalancedPersona();

/// Multi-panel, dense, pro-tools layout. Inspired by Unreal Engine.
[[nodiscard]] UIPersona MakeProDensePersona();

/// Technical persona — Pro defaults plus diagnostics surfaces enabled.
[[nodiscard]] UIPersona MakeTechnicalPersona();

/// Empty `Custom` slot — a copy of `IndieBalanced` with id changed
/// to `saga.persona.custom`. Persona authors fill in their own values
/// after construction and store the result through the registry.
[[nodiscard]] UIPersona MakeCustomPersona();

/// Stable canonical id for the built-in persona matching `tier`.
/// Returns `MakeIndieBalancedPersona().id` for `PersonaTier::Custom`
/// (the welcome screen falls back to it when the user has not yet
/// authored a custom persona).
[[nodiscard]] std::string DefaultPersonaIdForTier(PersonaTier tier);

} // namespace SagaEditor
