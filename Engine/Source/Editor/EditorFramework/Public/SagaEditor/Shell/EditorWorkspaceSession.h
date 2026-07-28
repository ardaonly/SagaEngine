// SPDX-License-Identifier: MPL-2.0
/// @file EditorWorkspaceSession.h
/// @brief Mountable editor mode facade for Saga-owned product windows.

#pragma once

#include "SagaEditor/Project/EditorProjectWorkflowController.h"

#include <memory>
#include <filesystem>
#include <functional>
#include <string>

namespace SagaEditor
{

class EditorHost;
class EditorShell;
class IEditorSettingsStore;
class IUIMainWindow;

// ─── Editor Mode Configuration ───────────────────────────────────────────────

/// Startup settings for embedding the editor shell into an existing product UI.
struct EditorWorkspaceSessionConfig
{
    std::string windowTitle = "Saga";
    int windowWidth = 1600;
    int windowHeight = 900;
    bool maximized = false;
    std::string workspacePath;
    std::string projectPath;
    std::string authoringModeId;
    std::filesystem::path runtimeExecutable;
    std::function<void()> returnHome;
    std::function<void()> workspaceChanged;
    ProjectWorkflowPrompts projectWorkflowPrompts;
    std::string layoutPreset = "Default";
    bool registerDefaultPanels = true;
    bool showOnMount = true;
};

// ─── Editor Mode ──────────────────────────────────────────────────────────────

/// Owns editor services while the product has editor mode mounted.
/// The product supplies the window wrapper and settings store; this class does
/// not create an application object or own the process event loop.
class EditorWorkspaceSession
{
public:
    EditorWorkspaceSession();
    ~EditorWorkspaceSession();

    EditorWorkspaceSession(const EditorWorkspaceSession&) = delete;
    EditorWorkspaceSession& operator=(const EditorWorkspaceSession&) = delete;
    EditorWorkspaceSession(EditorWorkspaceSession&&) noexcept;
    EditorWorkspaceSession& operator=(EditorWorkspaceSession&&) noexcept;

    /// Initialise editor services and attach the shell to the supplied window.
    [[nodiscard]] bool Mount(std::unique_ptr<IUIMainWindow> window,
                             std::unique_ptr<IEditorSettingsStore> settingsStore,
                             const EditorWorkspaceSessionConfig& config);

    /// Tear down the shell and editor services in reverse startup order.
    void Unmount();

    [[nodiscard]] bool IsMounted() const noexcept;

    [[nodiscard]] EditorHost* GetHost() noexcept;
    [[nodiscard]] const EditorHost* GetHost() const noexcept;

    [[nodiscard]] EditorShell* GetShell() noexcept;
    [[nodiscard]] const EditorShell* GetShell() const noexcept;
    [[nodiscard]] bool CanCloseWorkspace();

private:
    std::unique_ptr<EditorHost> m_host;   ///< Editor service registry for this mount.
    std::unique_ptr<EditorShell> m_shell; ///< Shell attached to the product window.
};

} // namespace SagaEditor
