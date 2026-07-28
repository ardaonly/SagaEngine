// SPDX-License-Identifier: MPL-2.0
/// @file EditorWorkspaceSession.cpp
/// @brief EditorWorkspaceSession implementation.

#include "SagaEditor/Shell/EditorWorkspaceSession.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <utility>

namespace SagaEditor
{

EditorWorkspaceSession::EditorWorkspaceSession() = default;

EditorWorkspaceSession::~EditorWorkspaceSession()
{
    Unmount();
}

EditorWorkspaceSession::EditorWorkspaceSession(EditorWorkspaceSession&&) noexcept = default;

EditorWorkspaceSession& EditorWorkspaceSession::operator=(EditorWorkspaceSession&& other) noexcept
{
    if (this != &other)
    {
        Unmount();
        m_host = std::move(other.m_host);
        m_shell = std::move(other.m_shell);
    }

    return *this;
}

bool EditorWorkspaceSession::Mount(std::unique_ptr<IUIMainWindow> window,
                       std::unique_ptr<IEditorSettingsStore> settingsStore,
                       const EditorWorkspaceSessionConfig& config)
{
    if (IsMounted() || !window || !settingsStore)
        return false;

    auto host = std::make_unique<EditorHost>();
    if (!host->Init(std::move(settingsStore), config.workspacePath))
        return false;

    if ((!config.authoringModeId.empty() &&
         !host->GetAuthoringModeService().SetActive(config.authoringModeId)) ||
        (!config.projectPath.empty() &&
         !host->GetDocumentWorkspace().OpenProject(config.projectPath)))
    {
        host->Shutdown();
        return false;
    }

    EditorShellConfig shellConfig;
    shellConfig.windowTitle = config.windowTitle;
    shellConfig.windowWidth = config.windowWidth;
    shellConfig.windowHeight = config.windowHeight;
    shellConfig.maximized = config.maximized;
    shellConfig.workspacePath = config.workspacePath;
    shellConfig.layoutPreset = config.layoutPreset;
    shellConfig.registerDefaultPanels = config.registerDefaultPanels;
    shellConfig.showOnInit = config.showOnMount;
    shellConfig.runtimeExecutable = config.runtimeExecutable;
    shellConfig.returnHome = config.returnHome;
    shellConfig.workspaceChanged = config.workspaceChanged;
    shellConfig.projectWorkflowPrompts = config.projectWorkflowPrompts;

    auto shell = std::make_unique<EditorShell>();
    if (!shell->Init(*host, std::move(window), shellConfig))
    {
        host->Shutdown();
        return false;
    }

    m_host = std::move(host);
    m_shell = std::move(shell);
    return true;
}

void EditorWorkspaceSession::Unmount()
{
    if (m_shell)
        m_shell->Shutdown();
    if (m_host)
        m_host->Shutdown();

    m_shell.reset();
    m_host.reset();
}

bool EditorWorkspaceSession::IsMounted() const noexcept
{
    return m_host != nullptr && m_shell != nullptr;
}

EditorHost* EditorWorkspaceSession::GetHost() noexcept
{
    return m_host.get();
}

const EditorHost* EditorWorkspaceSession::GetHost() const noexcept
{
    return m_host.get();
}

EditorShell* EditorWorkspaceSession::GetShell() noexcept
{
    return m_shell.get();
}

const EditorShell* EditorWorkspaceSession::GetShell() const noexcept
{
    return m_shell.get();
}

bool EditorWorkspaceSession::CanCloseWorkspace()
{
    return !m_shell || m_shell->CanCloseWorkspace();
}

} // namespace SagaEditor
