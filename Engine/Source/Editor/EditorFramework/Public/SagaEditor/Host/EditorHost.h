/// @file EditorHost.h
/// @brief Central service registry for all editor subsystems.

#pragma once

#include "SagaEditor/Composition/EditorCompositionSession.h"
#include "SagaEditor/Host/EditorWorkspaceDefinition.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

class CommandRegistry;
class EditorCustomizationCatalog;
class ShortcutCustomizationSession;
class WorkspaceCustomizationSession;
class CommandDispatcher;
class CommandPalette;
class ShortcutManager;
class UndoRedoStack;
class SelectionManager;
class ThemeRegistry;
class PersonaRegistry;
class PersonaActivator;
class PersonaSettingsBinding;
class EditorProfileRegistry;
class EditorProfileActivator;
class EditorProfileSettingsBinding;
class IEditorSettingsStore;
class ExtensionRegistry;
class ExtensionHost;
class IEditorEngineBridge;
class IEditorDiagnosticsService;
class EditorCompositionSession;
class EditorNotificationCenter;

// ─── Host ─────────────────────────────────────────────────────────────────────

/// Owns and exposes every editor subsystem through typed accessors. Panels,
/// shells, and extensions retrieve services here rather than through globals.
/// All subsystems are created in Init() and destroyed in Shutdown().
class EditorHost
{
public:
    EditorHost();
    ~EditorHost();

    /// Construct and initialise all subsystems. Returns false on failure.
    [[nodiscard]] bool Init(std::unique_ptr<IEditorSettingsStore> settingsStore = nullptr,
                            std::filesystem::path workspaceRoot = {},
                            EditorCompositionStartupConfig composition = {});

    /// Construct and initialise all subsystems from a prepared workspace input.
    [[nodiscard]] bool Init(std::unique_ptr<IEditorSettingsStore> settingsStore,
                            EditorWorkspaceDefinition workspace,
                            EditorCompositionStartupConfig composition = {});

    /// Shut down all subsystems in reverse-init order.
    void Shutdown();

    // ─── Command Services ─────────────────────────────────────────────────────

    /// Registry of every named editor command.
    [[nodiscard]] CommandRegistry&    GetCommandRegistry()    noexcept;

    /// Routes command invocations to registered handlers.
    [[nodiscard]] CommandDispatcher&  GetCommandDispatcher()  noexcept;

    /// Searchable overlay that lists and invokes commands by text.
    [[nodiscard]] CommandPalette&     GetCommandPalette()     noexcept;

    /// Manages keyboard shortcut bindings — fully remappable at runtime.
    [[nodiscard]] ShortcutManager&    GetShortcutManager()    noexcept;

    /// Transactional undo/redo stack shared across all editor actions.
    [[nodiscard]] UndoRedoStack&      GetUndoRedoStack()      noexcept;

    // ─── Scene Services ───────────────────────────────────────────────────────

    /// Tracks what objects are currently selected in the editor.
    [[nodiscard]] SelectionManager&   GetSelectionManager()   noexcept;

    // ─── Customisation Services ───────────────────────────────────────────────

    /// Registry of available editor themes — swap at runtime without restart.
    [[nodiscard]] ThemeRegistry&      GetThemeRegistry()      noexcept;

    /// Registry of available editor personas — built-in plus user-authored.
    [[nodiscard]] PersonaRegistry&    GetPersonaRegistry()    noexcept;

    /// Applies the active persona to wired editor subsystems.
    [[nodiscard]] PersonaActivator&   GetPersonaActivator()   noexcept;

    /// Registry of available workflow profiles — shell-level editor modes.
    [[nodiscard]] EditorProfileRegistry& GetEditorProfileRegistry() noexcept;

    /// Applies the active workflow profile to shell-level sinks.
    [[nodiscard]] EditorProfileActivator& GetEditorProfileActivator() noexcept;

    /// Private user settings backing editor preferences.
    [[nodiscard]] IEditorSettingsStore& GetSettingsStore()    noexcept;

    /// Editor-owned bridge to engine/runtime services.
    [[nodiscard]] IEditorEngineBridge& GetEngineBridge() noexcept;

    /// Shared diagnostics collection consumed by Problems and publish gates.
    [[nodiscard]] IEditorDiagnosticsService& GetEditorDiagnosticsService() noexcept;

    /// Transient user-facing notification hub for editor status feedback.
    [[nodiscard]] EditorNotificationCenter& GetEditorNotificationCenter() noexcept;
    [[nodiscard]] const EditorNotificationCenter&
        GetEditorNotificationCenter() const noexcept;

    /// Editor customization catalog and its load status.
    [[nodiscard]] EditorCustomizationCatalog& GetCustomizationCatalog() noexcept;

    /// Safe workspace customization session for future Customize Workspace UI.
    [[nodiscard]] WorkspaceCustomizationSession&
        GetWorkspaceCustomizationSession() noexcept;
    [[nodiscard]] const WorkspaceCustomizationSession&
        GetWorkspaceCustomizationSession() const noexcept;

    /// Safe shortcut customization session for Shortcut Preferences UI.
    [[nodiscard]] ShortcutCustomizationSession&
        GetShortcutCustomizationSession() noexcept;
    [[nodiscard]] const ShortcutCustomizationSession&
        GetShortcutCustomizationSession() const noexcept;

    /// Resolved compiled editor composition state for the current session.
    [[nodiscard]] EditorCompositionSession& GetCompositionSession() noexcept;
    [[nodiscard]] const EditorCompositionSession& GetCompositionSession() const noexcept;

    /// Refresh customization availability from shell-owned panel implementations.
    void RefreshWorkspaceCustomizationAvailability(
        std::vector<std::string> registeredPanelIds);

    /// Prepared workspace input currently consumed by the editor.
    [[nodiscard]] const std::optional<EditorWorkspaceDefinition>&
        GetWorkspaceDefinition() const noexcept;

    /// Manages installed editor extensions (panels, tools, commands).
    [[nodiscard]] ExtensionRegistry&  GetExtensionRegistry()  noexcept;

    /// Drives extension lifecycle: load, initialise, and unload.
    [[nodiscard]] ExtensionHost&      GetExtensionHost()      noexcept;

private:
    // ─── Owned Subsystems ─────────────────────────────────────────────────────

    std::unique_ptr<CommandRegistry>   m_commandRegistry;
    std::unique_ptr<CommandDispatcher> m_commandDispatcher;
    std::unique_ptr<CommandPalette>    m_commandPalette;
    std::unique_ptr<ShortcutManager>   m_shortcutManager;
    std::unique_ptr<UndoRedoStack>     m_undoRedoStack;
    std::unique_ptr<SelectionManager>  m_selectionManager;
    std::unique_ptr<ThemeRegistry>     m_themeRegistry;
    std::unique_ptr<PersonaRegistry>   m_personaRegistry;
    std::unique_ptr<PersonaActivator>  m_personaActivator;
    std::unique_ptr<PersonaSettingsBinding> m_personaSettingsBinding;
    std::unique_ptr<EditorProfileRegistry> m_editorProfileRegistry;
    std::unique_ptr<EditorProfileActivator> m_editorProfileActivator;
    std::unique_ptr<EditorProfileSettingsBinding> m_editorProfileSettingsBinding;
    std::unique_ptr<IEditorSettingsStore> m_settingsStore;
    std::unique_ptr<IEditorEngineBridge> m_engineBridge;
    std::unique_ptr<IEditorDiagnosticsService> m_editorDiagnosticsService;
    std::unique_ptr<EditorNotificationCenter> m_editorNotificationCenter;
    std::unique_ptr<EditorCustomizationCatalog> m_customizationCatalog;
    std::unique_ptr<ShortcutCustomizationSession> m_shortcutCustomizationSession;
    std::unique_ptr<WorkspaceCustomizationSession> m_workspaceCustomizationSession;
    std::unique_ptr<EditorCompositionSession> m_compositionSession;
    std::optional<EditorWorkspaceDefinition> m_workspaceDefinition;
    std::unique_ptr<ExtensionRegistry> m_extensionRegistry;
    std::unique_ptr<ExtensionHost>     m_extensionHost;
};

} // namespace SagaEditor
