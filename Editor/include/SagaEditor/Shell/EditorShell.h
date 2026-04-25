/// @file EditorShell.h
/// @brief Main editor shell — owns the main window, docking, menus, and panels.

#pragma once

#include "SagaEditor/Shell/ShellLayout.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{

class EditorHost;
class IPanel;
class IUIFactory;

// ─── Shell Configuration ──────────────────────────────────────────────────────

struct EditorShellConfig
{
    std::string windowTitle   = "SagaEditor";
    int         windowWidth   = 1600;
    int         windowHeight  = 900;
    bool        maximized     = false;
    std::string workspacePath = "";
    std::string layoutPreset  = "Default";
};

// ─── Editor Shell ─────────────────────────────────────────────────────────────

/// Owns the IUIMainWindow and all docked panels. Framework-free header — the
/// concrete window is created via IUIFactory and spoken to through IUIMainWindow.
/// Extensions contribute panels and menu entries before the window is shown.
class EditorShell
{
public:
    EditorShell();
    ~EditorShell();

    /// Build the main window (via factory), default panels, and chrome.
    /// Returns false if any step fails.
    [[nodiscard]] bool Init(EditorHost&              host,
                            IUIFactory&              factory,
                            const EditorShellConfig& cfg);

    /// Tear down panels and subsystems (called before the event loop exits).
    void Shutdown();

    // ─── Panel Management ─────────────────────────────────────────────────────

    /// Register a panel and dock it into the main window. Shell takes ownership.
    void RegisterPanel(std::unique_ptr<IPanel> panel,
                       UIDockArea              area = UIDockArea::Right);

    /// Find a registered panel by its stable PanelId.
    [[nodiscard]] IPanel* FindPanel(const std::string& panelId) const;

    // ─── Shell Chrome ─────────────────────────────────────────────────────────

    /// Replace the active shell layout descriptor and rebuild menus.
    void SetShellLayout(ShellLayout layout);

    [[nodiscard]] const ShellLayout& GetShellLayout() const noexcept;

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] IUIMainWindow& GetMainWindow() noexcept;
    [[nodiscard]] EditorHost&    GetHost()       noexcept;

private:
    // ─── Internals ────────────────────────────────────────────────────────────

    void RegisterDefaultPanels();
    void BuildDefaultLayout();

    EditorHost*                          m_host = nullptr; ///< Non-owning.
    std::unique_ptr<IUIMainWindow>       m_window;
    std::vector<std::unique_ptr<IPanel>> m_panels;         ///< All panels, owned.
    ShellLayout                          m_layout;
};

} // namespace SagaEditor
