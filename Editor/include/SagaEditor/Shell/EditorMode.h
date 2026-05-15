/// @file EditorMode.h
/// @brief Mountable editor mode facade for Saga-owned product windows.

#pragma once

#include <memory>
#include <string>

namespace SagaEditor
{

class EditorHost;
class EditorShell;
class IEditorSettingsStore;
class IUIMainWindow;

// ─── Editor Mode Configuration ───────────────────────────────────────────────

/// Startup settings for embedding the editor shell into an existing product UI.
struct EditorModeConfig
{
    std::string windowTitle = "SagaEditor";
    int windowWidth = 1600;
    int windowHeight = 900;
    bool maximized = false;
    std::string workspacePath;
    std::string layoutPreset = "Default";
    std::string initialProfileId;
    bool registerDefaultPanels = true;
    bool applyActivePersona = true;
    bool showOnMount = true;
};

// ─── Editor Mode ──────────────────────────────────────────────────────────────

/// Owns editor services while the product has editor mode mounted.
/// The product supplies the window wrapper and settings store; this class does
/// not create an application object or own the process event loop.
class EditorMode
{
public:
    EditorMode();
    ~EditorMode();

    EditorMode(const EditorMode&) = delete;
    EditorMode& operator=(const EditorMode&) = delete;
    EditorMode(EditorMode&&) noexcept;
    EditorMode& operator=(EditorMode&&) noexcept;

    /// Initialise editor services and attach the shell to the supplied window.
    [[nodiscard]] bool Mount(std::unique_ptr<IUIMainWindow> window,
                             std::unique_ptr<IEditorSettingsStore> settingsStore,
                             const EditorModeConfig& config);

    /// Tear down the shell and editor services in reverse startup order.
    void Unmount();

    [[nodiscard]] bool IsMounted() const noexcept;

    [[nodiscard]] EditorHost* GetHost() noexcept;
    [[nodiscard]] const EditorHost* GetHost() const noexcept;

    [[nodiscard]] EditorShell* GetShell() noexcept;
    [[nodiscard]] const EditorShell* GetShell() const noexcept;

private:
    std::unique_ptr<EditorHost> m_host;   ///< Editor service registry for this mount.
    std::unique_ptr<EditorShell> m_shell; ///< Shell attached to the product window.
};

} // namespace SagaEditor
