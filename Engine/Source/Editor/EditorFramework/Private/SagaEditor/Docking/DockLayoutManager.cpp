/// @file DockLayoutManager.cpp
/// @brief Saves, loads, and switches dock layouts and workspace presets.

#include "SagaEditor/Docking/DockLayoutManager.h"
#include "SagaEditor/Docking/DockWorkspace.h"
#include "SagaEditor/Layouts/LayoutSerializer.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

DockLayoutManager::DockLayoutManager(DockWorkspace& workspace,
                                     LayoutSerializer& serializer)
    : m_workspace(workspace), m_serializer(serializer)
{}

// ─── Layout Operations ────────────────────────────────────────────────────────

bool DockLayoutManager::SaveCurrentLayout(const std::string& presetName)
{
    m_activePreset = presetName;
    return m_serializer.SaveLayout(m_workspace, presetName);
}

bool DockLayoutManager::LoadLayout(const std::string& presetName)
{
    if (!m_serializer.LoadLayout(m_workspace, presetName))
        return false;

    m_activePreset = presetName;
    return true;
}

void DockLayoutManager::ResetToDefault()
{
    m_workspace.ResetToDefaultLayout();
    m_activePreset = "Default";
}

std::vector<LayoutPreset> DockLayoutManager::ListAvailableLayouts() const
{
    return m_serializer.ListLayouts();
}

const std::string& DockLayoutManager::GetActivePreset() const noexcept
{
    return m_activePreset;
}

} // namespace SagaEditor
