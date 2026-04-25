/// @file LayoutSerializer.h
/// @brief Save and load dock layouts and workspace presets to/from JSON.

#pragma once

#include "SagaEditor/Layouts/LayoutPreset.h"
#include "SagaEditor/Layouts/WorkspacePreset.h"
#include <string>
#include <vector>
#include <optional>

namespace SagaEditor
{

class DockWorkspace;

// ─── Layout Serializer ────────────────────────────────────────────────────────

/// Reads and writes LayoutPreset and WorkspacePreset JSON files.
/// Files are stored under <workspacePath>/Layouts/.
class LayoutSerializer
{
public:
    explicit LayoutSerializer(std::string layoutsDir);

    // ─── Layout Presets ───────────────────────────────────────────────────────

    /// Capture the current dock arrangement as a preset and save it to disk.
    [[nodiscard]] bool SaveLayout(const DockWorkspace& workspace,
                                  const std::string&   presetName);

    /// Load a LayoutPreset from disk and apply it to the workspace.
    [[nodiscard]] bool LoadLayout(DockWorkspace&     workspace,
                                  const std::string& presetName);

    /// List all LayoutPresets found in the layouts directory.
    [[nodiscard]] std::vector<LayoutPreset> ListLayouts() const;

    // ─── Workspace Presets ────────────────────────────────────────────────────

    /// Persist a WorkspacePreset to its JSON file.
    [[nodiscard]] bool SaveWorkspacePreset(const WorkspacePreset& preset);

    /// Load a WorkspacePreset by name. Returns nullopt if not found.
    [[nodiscard]] std::optional<WorkspacePreset> LoadWorkspacePreset(
        const std::string& presetName) const;

    /// List all WorkspacePresets in the layouts directory.
    [[nodiscard]] std::vector<WorkspacePreset> ListWorkspacePresets() const;

private:
    std::string m_layoutsDir; ///< Root directory for all layout JSON files.
};

} // namespace SagaEditor
