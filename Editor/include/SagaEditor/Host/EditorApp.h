/// @file EditorApp.h
/// @brief Top-level editor application entry point — owns the UI backend, host, and shell.

#pragma once

#include <memory>
#include <string>

namespace SagaEditor
{

class EditorHost;
class EditorShell;
class IUIApplication;
class IUIFactory;

// ─── Configuration ────────────────────────────────────────────────────────────

/// Startup parameters forwarded from the platform entry point (main / WinMain).
struct EditorAppConfig
{
    int         argc          = 0;
    char**      argv          = nullptr;
    std::string windowTitle   = "SagaEditor";
    int         windowWidth   = 1600;
    int         windowHeight  = 900;
    bool        maximized     = false;
    std::string workspacePath = "";           ///< Project workspace root; empty = cwd.
    std::string layoutPreset  = "Default";    ///< Layout preset applied on first launch.
};

// ─── Application ─────────────────────────────────────────────────────────────

/// Bootstraps the UI backend (via IUIFactory), creates EditorHost and
/// EditorShell, then runs the event loop. No UI framework type appears here;
/// the concrete factory is injected in Init().
class EditorApp
{
public:
    EditorApp();
    ~EditorApp();

    /// Initialise the UI backend, host services, and the main window.
    /// Pass a concrete IUIFactory to select the backend (e.g. QtUIFactory).
    /// Returns false if any critical subsystem fails.
    [[nodiscard]] bool Init(const EditorAppConfig& cfg, IUIFactory& factory);

    /// Enter the framework event loop. Blocks until the main window is closed.
    /// Returns the backend exit code.
    int Run();

    /// Shut down all subsystems in reverse-init order.
    void Shutdown();

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] EditorHost&  GetHost()  noexcept;
    [[nodiscard]] EditorShell& GetShell() noexcept;

private:
    std::unique_ptr<IUIApplication> m_uiApp;  ///< Owns the event loop.
    std::unique_ptr<EditorHost>     m_host;
    std::unique_ptr<EditorShell>    m_shell;
};

} // namespace SagaEditor
