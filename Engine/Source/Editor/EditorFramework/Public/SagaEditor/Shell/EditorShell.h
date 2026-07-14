/// @file EditorShell.h
/// @brief Main editor shell — owns the main window, docking, menus, and panels.

#pragma once

#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Shell/ShellLayout.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

struct ResolvedEditorCompositionSnapshot;
class EditorHost;
class EditorProfile;
class IPanel;
class IUIFactory;
class ProductionDashboardPanel;

// ─── Shell Configuration ──────────────────────────────────────────────────────

struct EditorShellConfig
{
    std::string windowTitle   = "SagaEditor";
    int         windowWidth   = 1600;
    int         windowHeight  = 900;
    bool        maximized     = false;
    std::string workspacePath = "";
    std::string layoutPreset  = "Default";
    bool        registerDefaultPanels = true;
    bool        applyActivePersona = true;
    bool        showOnInit = true;
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

    /// Build the shell around a pre-existing backend window owned by a product
    /// host. Used when Saga mounts editor mode inside its own process/window.
    [[nodiscard]] bool Init(EditorHost&                    host,
                            std::unique_ptr<IUIMainWindow> window,
                            const EditorShellConfig&       cfg);

    /// Tear down panels and subsystems (called before the event loop exits).
    void Shutdown();

    // ─── Panel Management ─────────────────────────────────────────────────────

    /// Register a panel and dock it into the main window. Shell takes ownership.
    void RegisterPanel(std::unique_ptr<IPanel> panel,
                       UIDockArea              area = UIDockArea::Right);

    /// Find a registered panel by its stable PanelId.
    [[nodiscard]] IPanel* FindPanel(const std::string& panelId) const;

    /// Return the current panel titles in registration order.
    [[nodiscard]] std::vector<std::string> GetRegisteredPanelTitles() const;

    // ─── Shell Chrome ─────────────────────────────────────────────────────────

    /// Replace the active shell layout descriptor and rebuild menus.
    void SetShellLayout(ShellLayout layout);

    [[nodiscard]] const ShellLayout& GetShellLayout() const noexcept;

    [[nodiscard]] const std::string& GetActiveLayoutPresetId() const noexcept;
    [[nodiscard]] const std::vector<std::string>& GetActiveToolbarCommands() const noexcept;
    [[nodiscard]] const std::vector<std::string>& GetActiveToolCommands() const noexcept;

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] IUIMainWindow& GetMainWindow() noexcept;
    [[nodiscard]] EditorHost&    GetHost()       noexcept;

private:
    // ─── Internals ────────────────────────────────────────────────────────────

    void RegisterDefaultPanels();
    void BuildDefaultLayout();
    bool BuildCompositionLayout();
    bool ApplyCompositionShortcuts();
    bool ApplyCompositionPanelVisibility();
    void MergeCompositionShellDiagnostics(std::vector<EditorDiagnostic> diagnostics);
    [[nodiscard]] const ResolvedEditorCompositionSnapshot* GetUsableCompositionSnapshot() const;
    [[nodiscard]] std::vector<std::string> GetRegisteredPanelIds() const;
    [[nodiscard]] std::optional<bool> GetCompositionPanelVisibility(
        const std::string& panelId) const;
    [[nodiscard]] UIDockArea ResolveCompositionDockArea(
        const std::string& panelId,
        UIDockArea fallback) const;
    void RegisterPersonaCommands();
    void RegisterProfileCommands();
    void WirePersonaSinks();
    void WireProfileSinks();
    void WireNotificationSink();
    void ApplyActivePersona();
    void ApplyActiveProfile();
    void RefreshProductionDashboard();
    bool ApplyPersonaPanelVisibility(const std::vector<std::string>& panelIds);
    bool ApplyProfileLayout(const std::string& layoutPresetId);
    bool ApplyProfileShortcutMap(const EditorProfile& profile);
    bool ApplyProfileToolbar(const std::vector<std::string>& commandIds);
    bool ApplyProfilePanelVisibility(const std::vector<std::string>& panelIds);
    bool ApplyProfileShellChrome(const EditorProfile& profile);
    bool ApplyProfileToolVisibility(const std::vector<std::string>& commandIds);

    EditorHost*                          m_host = nullptr; ///< Non-owning.
    std::unique_ptr<IUIMainWindow>       m_window;
    std::vector<std::unique_ptr<IPanel>> m_panels;         ///< All panels, owned.
    ProductionDashboardPanel*            m_productionDashboard = nullptr;
    ShellLayout                          m_layout;
    std::string                          m_activeLayoutPresetId = "Default";
    std::vector<std::string>             m_activeToolbarCommands;
    std::vector<std::string>             m_activeToolCommands;
    std::vector<EditorDiagnostic>        m_compositionShellDiagnostics;
    std::string                          m_baseWindowTitle = "SagaEditor";
    PersonaChangeSubscription            m_personaSubscription =
        kInvalidPersonaSubscription;
    EditorProfileChangeSubscription      m_profileSubscription =
        kInvalidEditorProfileSubscription;
    EditorNotificationSubscription       m_notificationSubscription =
        kInvalidEditorNotificationSubscription;
};

} // namespace SagaEditor
