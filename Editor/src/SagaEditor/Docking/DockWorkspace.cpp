/// @file DockWorkspace.cpp
/// @brief DockWorkspace implementation.

#include "SagaEditor/Docking/DockWorkspace.h"
#include "SagaEditor/Panels/IPanel.h"

namespace SagaEditor
{

DockWorkspace::DockWorkspace(IUIMainWindow& window)
    : m_window(window)
{}

// ─── Panel Management ─────────────────────────────────────────────────────────

void DockWorkspace::AddPanel(IPanel* panel, UIDockArea area)
{
    m_window.DockPanel(panel->GetNativeWidget(),
                       panel->GetPanelId(),
                       panel->GetTitle(),
                       area);
}

void DockWorkspace::RemovePanel(const std::string& panelId)
{
    m_window.UndockPanel(panelId);
}

void DockWorkspace::FocusPanel(const std::string& panelId)
{
    m_window.FocusPanel(panelId);
}

// ─── Layout Persistence ───────────────────────────────────────────────────────

std::vector<uint8_t> DockWorkspace::SaveState() const
{
    return m_window.SaveState();
}

bool DockWorkspace::RestoreState(const std::vector<uint8_t>& state)
{
    return m_window.RestoreState(state);
}

} // namespace SagaEditor
