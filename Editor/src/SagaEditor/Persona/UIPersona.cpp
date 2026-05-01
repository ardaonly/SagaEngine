/// @file UIPersona.cpp
/// @brief Implementation of the top-level persona descriptor + factories.

#include "SagaEditor/Persona/UIPersona.h"

namespace SagaEditor
{

// ─── Persona Tier Identity ────────────────────────────────────────────────────

const char* PersonaTierId(PersonaTier tier) noexcept
{
    switch (tier)
    {
        case PersonaTier::Beginner:  return "saga.tier.beginner";
        case PersonaTier::Indie:     return "saga.tier.indie";
        case PersonaTier::Pro:       return "saga.tier.pro";
        case PersonaTier::Technical: return "saga.tier.technical";
        case PersonaTier::Custom:    return "saga.tier.custom";
    }
    return "saga.tier.unknown";
}

PersonaTier PersonaTierFromId(const std::string& id) noexcept
{
    if (id == "saga.tier.beginner")  return PersonaTier::Beginner;
    if (id == "saga.tier.indie")     return PersonaTier::Indie;
    if (id == "saga.tier.pro")       return PersonaTier::Pro;
    if (id == "saga.tier.technical") return PersonaTier::Technical;
    if (id == "saga.tier.custom")    return PersonaTier::Custom;
    return PersonaTier::Indie;
}

// ─── Equality ─────────────────────────────────────────────────────────────────

bool UIPersona::operator==(const UIPersona& o) const noexcept
{
    return id                  == o.id
        && displayName         == o.displayName
        && description         == o.description
        && tier                == o.tier
        && themeId             == o.themeId
        && workspacePresetId   == o.workspacePresetId
        && shortcutMapId       == o.shortcutMapId
        && density             == o.density
        && blockStyle          == o.blockStyle
        && defaultPanels       == o.defaultPanels
        && defaultToolbarCommands == o.defaultToolbarCommands
        && exposesGraphEditor  == o.exposesGraphEditor
        && exposesProfiler     == o.exposesProfiler
        && exposesConsole      == o.exposesConsole
        && exposesAssetBrowser == o.exposesAssetBrowser
        && showShortcutHints   == o.showShortcutHints;
}

// ─── Built-in Persona Factories ───────────────────────────────────────────────

UIPersona MakeBlockyBeginnerPersona()
{
    UIPersona p;
    p.id          = "saga.persona.beginner";
    p.displayName = "Blocky Beginner";
    p.description = "Kid-friendly stacked blocks. Big buttons, big text, "
                    "no exposed pins. Inspired by Scratch.";
    p.tier        = PersonaTier::Beginner;

    p.themeId           = "Light";
    p.workspacePresetId = "creator";
    p.shortcutMapId     = "saga.shortcuts.beginner";

    p.density    = MakeCosyDensity();
    p.blockStyle = MakeStackedBlocksStyle();

    p.defaultPanels = {
        "saga.panel.hierarchy",
        "saga.panel.viewport",
        "saga.panel.graph",
        "saga.panel.console",
    };
    p.defaultToolbarCommands = {
        "saga.command.world.play",
        "saga.command.world.stop",
        "saga.command.file.save",
    };
    p.exposesGraphEditor   = true;
    p.exposesProfiler      = false;
    p.exposesConsole       = true;
    p.exposesAssetBrowser  = false;
    p.showShortcutHints    = false;
    return p;
}

UIPersona MakeIndieBalancedPersona()
{
    UIPersona p;
    p.id          = "saga.persona.indie";
    p.displayName = "Indie Balanced";
    p.description = "Mid-density hybrid layout for indie developers. "
                    "Blocks and pin nodes coexist.";
    p.tier        = PersonaTier::Indie;

    p.themeId           = "Dark";
    p.workspacePresetId = "indie";
    p.shortcutMapId     = "saga.shortcuts.standard";

    p.density    = MakeComfortableDensity();
    p.blockStyle = MakeHybridStyle();

    p.defaultPanels = {
        "saga.panel.hierarchy",
        "saga.panel.viewport",
        "saga.panel.inspector",
        "saga.panel.console",
        "saga.panel.assetbrowser",
        "saga.panel.graph",
    };
    p.defaultToolbarCommands = {
        "saga.command.world.play",
        "saga.command.world.pause",
        "saga.command.world.stop",
        "saga.command.file.save",
        "saga.command.edit.undo",
        "saga.command.edit.redo",
    };
    p.exposesGraphEditor  = true;
    p.exposesProfiler     = true;
    p.exposesConsole      = true;
    p.exposesAssetBrowser = true;
    p.showShortcutHints   = true;
    return p;
}

UIPersona MakeProDensePersona()
{
    UIPersona p;
    p.id          = "saga.persona.pro";
    p.displayName = "Pro Dense";
    p.description = "Multi-panel pro-tools layout. Pin-based graphs, "
                    "low-saturation theme. Inspired by Unreal Engine.";
    p.tier        = PersonaTier::Pro;

    p.themeId           = "Midnight";
    p.workspacePresetId = "studio";
    p.shortcutMapId     = "saga.shortcuts.pro";

    p.density    = MakeCompactDensity();
    p.blockStyle = MakePinNodeStyle();

    p.defaultPanels = {
        "saga.panel.hierarchy",
        "saga.panel.viewport",
        "saga.panel.inspector",
        "saga.panel.console",
        "saga.panel.assetbrowser",
        "saga.panel.profiler",
        "saga.panel.graph",
        "saga.panel.collaboration",
    };
    p.defaultToolbarCommands = {
        "saga.command.world.play",
        "saga.command.world.pause",
        "saga.command.world.stop",
        "saga.command.world.step",
        "saga.command.build",
        "saga.command.edit.mode.translate",
        "saga.command.edit.mode.rotate",
        "saga.command.edit.mode.scale",
        "saga.command.edit.undo",
        "saga.command.edit.redo",
    };
    p.exposesGraphEditor  = true;
    p.exposesProfiler     = true;
    p.exposesConsole      = true;
    p.exposesAssetBrowser = true;
    p.showShortcutHints   = true;
    return p;
}

UIPersona MakeTechnicalPersona()
{
    UIPersona p = MakeProDensePersona();
    p.id          = "saga.persona.technical";
    p.displayName = "Technical";
    p.description = "Pro defaults plus profiler, network monitor, and "
                    "frame-graph diagnostics enabled by default.";
    p.tier        = PersonaTier::Technical;

    p.density    = MakeDenseDensity();
    p.blockStyle = MakeCompactPinStyle();

    p.workspacePresetId = "technical";
    p.defaultPanels.push_back("saga.panel.network");
    p.defaultPanels.push_back("saga.panel.framegraph");
    p.exposesProfiler   = true;
    p.showShortcutHints = true;
    return p;
}

UIPersona MakeCustomPersona()
{
    UIPersona p = MakeIndieBalancedPersona();
    p.id          = "saga.persona.custom";
    p.displayName = "Custom";
    p.description = "User-authored persona. Edit fields and save to "
                    "<workspace>/Personas/.";
    p.tier        = PersonaTier::Custom;
    p.density.step        = DensityStep::Custom;
    p.density.displayName = "Custom";
    return p;
}

std::string DefaultPersonaIdForTier(PersonaTier tier)
{
    switch (tier)
    {
        case PersonaTier::Beginner:  return MakeBlockyBeginnerPersona().id;
        case PersonaTier::Indie:     return MakeIndieBalancedPersona().id;
        case PersonaTier::Pro:       return MakeProDensePersona().id;
        case PersonaTier::Technical: return MakeTechnicalPersona().id;
        case PersonaTier::Custom:    return MakeIndieBalancedPersona().id;
    }
    return MakeIndieBalancedPersona().id;
}

} // namespace SagaEditor
