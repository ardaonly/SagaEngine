// SPDX-License-Identifier: MPL-2.0
/// @file EditorShellPanelLifecycle.cpp
/// @brief Removal side of editor panel ownership and dock lifecycle.

#include "SagaEditor/Shell/EditorShell.h"

#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <algorithm>

namespace SagaEditor
{

bool EditorShell::UnregisterPanel(const std::string& panelId)
{
    const auto it = std::find_if(
        m_panels.begin(), m_panels.end(),
        [&panelId](const auto& panel) { return panel->GetPanelId() == panelId; });
    if (it == m_panels.end())
        return false;

    m_window->UndockPanel(panelId);
    (*it)->OnShutdown();
    const std::pair<const char*, const char*> panelCommands[] = {
        {"saga.panel.hierarchy", "saga.command.view.hierarchy"},
        {"saga.panel.inspector", "saga.command.view.inspector"},
        {"saga.panel.assetbrowser", "saga.command.view.project_sources"},
        {"saga.panel.code_box", "saga.command.view.code_box"},
        {"saga.panel.problems", "saga.command.view.problems"},
        {"saga.panel.packages", "saga.command.view.packages"},
    };
    for (const auto& [ownedPanel, command] : panelCommands)
        if (panelId == ownedPanel)
            m_host->GetCommandRegistry().Unregister(command);

    m_panels.erase(it);
    return true;
}

} // namespace SagaEditor
