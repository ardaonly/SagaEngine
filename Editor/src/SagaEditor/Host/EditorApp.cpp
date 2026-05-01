/// @file EditorApp.cpp
/// @brief Top-level editor application entry point — owns the UI backend, host, and shell.

#include "SagaEditor/Host/EditorApp.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIApplication.h"
#include "SagaEditor/UI/IUIFactory.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

EditorApp::EditorApp()  = default;
EditorApp::~EditorApp() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorApp::Init(const EditorAppConfig& cfg, IUIFactory& factory)
{
    // UI application must be created before any window (mirrors QApplication rule).
    // `IUIFactory::CreateApplication` takes `int& argc` because the underlying
    // toolkit (Qt today) may consume processed arguments and trim the array.
    // The `cfg.argc` member is `const int`, so we mirror it into a local
    // mutable variable that the factory is allowed to scribble on.
    m_argc  = cfg.argc;
    m_argv  = cfg.argv;
    m_uiApp = factory.CreateApplication(m_argc, m_argv);

    m_host  = std::make_unique<EditorHost>();
    m_shell = std::make_unique<EditorShell>();

    if (!m_host->Init())
        return false;

    EditorShellConfig shellCfg;
    shellCfg.windowTitle   = cfg.windowTitle;
    shellCfg.windowWidth   = cfg.windowWidth;
    shellCfg.windowHeight  = cfg.windowHeight;
    shellCfg.maximized     = cfg.maximized;
    shellCfg.workspacePath = cfg.workspacePath;
    shellCfg.layoutPreset  = cfg.layoutPreset;

    if (!m_shell->Init(*m_host, factory, shellCfg))
        return false;

    return true;
}

int EditorApp::Run()
{
    return m_uiApp->Run();
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
