/// @file DockLayoutManager.h
/// @brief Saves, loads, and switches dock layouts and workspace presets.

#pragma once

#include "SagaEditor/Layouts/LayoutPreset.h"
#include <string>
#include <vector>

namespace SagaEditor
{

class DockWorkspace;
class LayoutSerializer;

// ─── Dock Layout Manager ─────────────────────────────────────────────────────

/// Bridges the DockWorkspace and LayoutSerializer so the shell and extension
/// panels can switch layouts, save the current arrangement, and query what
/// presets are available — all without touching serialization details.
class DockLayoutManager
{
public:
    DockLayoutManager(DockWorkspace& workspace, LayoutSerializer& serializer);

    /// Capture and persist the current dock arrangement under the given name.
    [[nodiscard]] bool SaveCurrentLayout(const std::string& presetName);

    /// Load a saved layout from disk and apply it to the workspace.
    [[nodiscard]] bool LoadLayout(const std::string& presetName);

    /// Discard the current arrangement and rebuild the default layout.
    void ResetToDefault();

    /// Return all LayoutPresets available in the layouts directory.
    [[nodiscard]] std::vector<LayoutPreset> ListAvailableLayouts() const;

    /// Return the name of the currently active preset.
    [[nodiscard]] const std::string& GetActivePreset() const noexcept;

private:
    DockWorkspace&    m_workspace;     ///< Non-owning reference.
    LayoutSerializer& m_serializer;    ///< Non-owning reference.
    std::string       m_activePreset = "Default"; ///< Name of the last applied preset.
};

} // namespace SagaEditor
