// SPDX-License-Identifier: MPL-2.0
/// @file EditorApp.cpp
/// @brief Owns editor host and shell state beneath the platform composition root.

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

EditorApp::EditorApp()  = default;
EditorApp::~EditorApp() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorApp::Init(
    const EditorAppConfig& cfg,
    std::unique_ptr<IEditorSettingsStore> settingsStore,
    std::unique_ptr<IUIMainWindow> mainWindow)
{
    if (!settingsStore || !mainWindow)
        return false;

    m_host  = std::make_unique<EditorHost>();
    m_shell = std::make_unique<EditorShell>();

    std::string resolvedWorkspacePath = cfg.workspacePath;
    std::string resolvedProfileId = cfg.initialProfileId;
    std::string resolvedLayoutPreset = cfg.layoutPreset;
    if (cfg.preparedWorkspace)
    {
        resolvedWorkspacePath = cfg.preparedWorkspace->root.string();
        if (!cfg.preparedWorkspace->initialProfileId.empty())
        {
            resolvedProfileId = cfg.preparedWorkspace->initialProfileId;
        }
        if (!cfg.preparedWorkspace->layoutPreset.empty())
        {
            resolvedLayoutPreset = cfg.preparedWorkspace->layoutPreset;
        }
        if (!m_host->Init(std::move(settingsStore),
                          *cfg.preparedWorkspace,
                          cfg.composition))
            return false;
    }
    else if (!m_host->Init(std::move(settingsStore),
                           resolvedWorkspacePath,
                           cfg.composition))
    {
        return false;
    }
    if (!resolvedProfileId.empty() &&
        !m_host->GetEditorProfileRegistry().SetActive(resolvedProfileId))
    {
        return false;
    }
    if (!cfg.projectPath.empty() &&
        !m_host->GetDocumentWorkspace().OpenProject(cfg.projectPath))
    {
        return false;
    }

    EditorShellConfig shellCfg;
    shellCfg.windowTitle   = cfg.windowTitle;
    shellCfg.windowWidth   = cfg.windowWidth;
    shellCfg.windowHeight  = cfg.windowHeight;
    shellCfg.maximized     = cfg.maximized;
    shellCfg.workspacePath = resolvedWorkspacePath;
    shellCfg.layoutPreset  = resolvedLayoutPreset;
    shellCfg.showOnInit    = cfg.showOnInit;
    shellCfg.registerDefaultPanels = cfg.registerDefaultPanels;
    shellCfg.applyActivePersona = cfg.applyActivePersona;
    shellCfg.projectWorkflowPrompts = cfg.projectWorkflowPrompts;
    shellCfg.runtimeExecutable = cfg.runtimeExecutable;

    if (!m_shell->Init(*m_host, std::move(mainWindow), shellCfg))
        return false;

    return true;
}

void EditorApp::Shutdown()
{
    if (m_shell)
        m_shell->Shutdown();

    if (m_host)
        m_host->Shutdown();
}

// ─── Accessors ────────────────────────────────────────────────────────────────

EditorHost&  EditorApp::GetHost()  noexcept { return *m_host;  }
EditorShell& EditorApp::GetShell() noexcept { return *m_shell; }

} // namespace SagaEditor
