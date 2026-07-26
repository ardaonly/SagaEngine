// SPDX-License-Identifier: MPL-2.0
/// @file EditorApp.h
/// @brief Owns editor host and shell state beneath the platform composition root.

#pragma once

#include "SagaEditor/Composition/EditorCompositionSession.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"
#include "SagaEditor/Project/EditorProjectWorkflowController.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace SagaEditor
{

class EditorHost;
class EditorShell;
class IEditorSettingsStore;
class IUIMainWindow;

// ─── Configuration ────────────────────────────────────────────────────────────

/// Startup parameters forwarded from the platform entry point (main / WinMain).
struct EditorAppConfig
{
    std::string windowTitle   = "SagaEditor";
    int         windowWidth   = 1600;
    int         windowHeight  = 900;
    bool        maximized     = false;
    bool        showOnInit    = true;
    bool        registerDefaultPanels = true;
    bool        applyActivePersona = true;
    std::string workspacePath = "";           ///< Project workspace root; empty = cwd.
    std::string projectPath = "";             ///< Canonical .sagaproj or containing directory.
    std::string layoutPreset  = "Default";    ///< Layout preset applied on first launch.
    std::string initialProfileId = "";        ///< Optional workflow profile override.
    std::filesystem::path runtimeExecutable;  ///< Sibling runtime resolved by composition.
    std::optional<EditorWorkspaceDefinition> preparedWorkspace;
    EditorCompositionStartupConfig composition; ///< Compiled composition startup input.
    ProjectWorkflowPrompts projectWorkflowPrompts;
};

// ─── Application ─────────────────────────────────────────────────────────────

/// Owns editor services and shell state after the platform composition root has
/// created the application, settings store, and typed window.
class EditorApp
{
public:
    EditorApp();
    ~EditorApp();

    /// Initialise host services around platform-owned concrete dependencies.
    [[nodiscard]] bool Init(
        const EditorAppConfig& cfg,
        std::unique_ptr<IEditorSettingsStore> settingsStore,
        std::unique_ptr<IUIMainWindow> mainWindow);

    /// Shut down all subsystems in reverse-init order.
    void Shutdown();

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] EditorHost&  GetHost()  noexcept;
    [[nodiscard]] EditorShell& GetShell() noexcept;

private:
    std::unique_ptr<EditorHost>     m_host;
    std::unique_ptr<EditorShell>    m_shell;
};

} // namespace SagaEditor
