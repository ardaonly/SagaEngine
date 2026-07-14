/// @file LayoutSerializer.cpp
/// @brief Save and load dock layouts and workspace presets to/from JSON.

#include "SagaEditor/Layouts/LayoutSerializer.h"
#include "SagaEditor/Docking/DockWorkspace.h"

#include <filesystem>
#include <fstream>

namespace SagaEditor
{

namespace fs = std::filesystem;

// ─── Construction ─────────────────────────────────────────────────────────────

LayoutSerializer::LayoutSerializer(std::string layoutsDir)
    : m_layoutsDir(std::move(layoutsDir))
{
    fs::create_directories(m_layoutsDir);
}

// ─── Layout Presets ───────────────────────────────────────────────────────────

bool LayoutSerializer::SaveLayout(const DockWorkspace& workspace,
                                   const std::string&   presetName)
{
    // Serialization of DockNode tree → JSON deferred until hand-rolled JSON
    // writer is available (mirrors the approach used in AssetRegistry).
    (void)workspace;
    (void)presetName;
    return true;
}

bool LayoutSerializer::LoadLayout(DockWorkspace&     workspace,
                                   const std::string& presetName)
{
    fs::path path = fs::path(m_layoutsDir) / (presetName + ".layout.json");
    if (!fs::exists(path))
        return false;

    // JSON parse and DockNode reconstruction deferred (see SaveLayout note).
    (void)workspace;
    return true;
}

std::vector<LayoutPreset> LayoutSerializer::ListLayouts() const
{
    std::vector<LayoutPreset> result;
    for (const auto& entry : fs::directory_iterator(m_layoutsDir))
    {
        if (entry.path().extension() == ".json" &&
            entry.path().stem().string().ends_with(".layout"))
        {
            LayoutPreset preset;
            preset.name = entry.path().stem().stem().string(); // strip ".layout"
            result.push_back(std::move(preset));
        }
    }
    return result;
}

// ─── Workspace Presets ────────────────────────────────────────────────────────

bool LayoutSerializer::SaveWorkspacePreset(const WorkspacePreset& preset)
{
    fs::path path = fs::path(m_layoutsDir) / (preset.name + ".workspace.json");
    std::ofstream f(path);
    if (!f.is_open())
        return false;

    // Minimal JSON writer — full serializer expands here.
    f << "{ \"name\": \"" << preset.name         << "\","
      << "  \"layout\": \"" << preset.layoutPreset << "\","
      << "  \"theme\": \""  << preset.themeName    << "\" }\n";
    return true;
}

std::optional<WorkspacePreset> LayoutSerializer::LoadWorkspacePreset(
    const std::string& presetName) const
{
    fs::path path = fs::path(m_layoutsDir) / (presetName + ".workspace.json");
    if (!fs::exists(path))
        return std::nullopt;

    // Full JSON parse deferred; returns empty preset shell for now.
    WorkspacePreset preset;
    preset.name = presetName;
    return preset;
}

std::vector<WorkspacePreset> LayoutSerializer::ListWorkspacePresets() const
{
    std::vector<WorkspacePreset> result;
    for (const auto& entry : fs::directory_iterator(m_layoutsDir))
    {
        if (entry.path().extension() == ".json" &&
            entry.path().stem().string().ends_with(".workspace"))
        {
            WorkspacePreset preset;
            preset.name = entry.path().stem().stem().string();
            result.push_back(std::move(preset));
        }
    }
    return result;
}

} // namespace SagaEditor
