/// @file LayoutPreset.h
/// @brief A named snapshot of the dock tree used as a switchable layout preset.

#pragma once

#include <string>
#include <vector>

namespace SagaEditor
{

// ─── Serialized Tab Entry ─────────────────────────────────────────────────────

/// Flat representation of one tab position, used in saved layout files.
struct LayoutTabEntry
{
    std::string panelId;     ///< Stable PanelId of the panel.
    uint32_t    nodeId  = 0; ///< Dock node this tab belongs to.
    bool        visible = true; ///< Whether the panel is visible in this preset.
};

// ─── Layout Preset ────────────────────────────────────────────────────────────

/// A named dock arrangement that users can save, load, and switch between
/// without losing their work. Presets ship as JSON under Layouts/.
struct LayoutPreset
{
    std::string                  name;         ///< Display name shown in the layout picker.
    std::string                  description;  ///< One-sentence description.
    std::string                  thumbnailPath; ///< Optional preview image path.
    std::vector<LayoutTabEntry>  tabs;         ///< All tab placements for this preset.
    bool                         builtin = false; ///< True for engine-shipped presets.
};

} // namespace SagaEditor
