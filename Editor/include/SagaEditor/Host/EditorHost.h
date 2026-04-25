/// @file EditorHost.h
/// @brief Central service registry for all editor subsystems.

#pragma once

#include <memory>

namespace SagaEditor
{

class CommandRegistry;
class CommandDispatcher;
class CommandPalette;
class ShortcutManager;
class UndoRedoStack;
class SelectionManager;
class ThemeRegistry;
class ExtensionRegistry;
class ExtensionHost;

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
    [[nodiscard]] bool Init();

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
    std::unique_ptr<ExtensionRegistry> m_extensionRegistry;
    std::unique_ptr<ExtensionHost>     m_extensionHost;
};

} // namespace SagaEditor
