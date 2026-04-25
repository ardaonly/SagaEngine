/// @file EditorHost.cpp
/// @brief Central service registry for all editor subsystems.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandPalette.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/ExtensionHost.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

EditorHost::EditorHost()  = default;
EditorHost::~EditorHost() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorHost::Init()
{
    m_commandRegistry   = std::make_unique<CommandRegistry>();
    m_commandDispatcher = std::make_unique<CommandDispatcher>(*m_commandRegistry);
    m_commandPalette    = std::make_unique<CommandPalette>(*m_commandRegistry);
    m_shortcutManager   = std::make_unique<ShortcutManager>(*m_commandDispatcher);
    m_undoRedoStack     = std::make_unique<UndoRedoStack>();
    m_selectionManager  = std::make_unique<SelectionManager>();
    m_themeRegistry     = std::make_unique<ThemeRegistry>();
    m_extensionRegistry = std::make_unique<ExtensionRegistry>();
    m_extensionHost     = std::make_unique<ExtensionHost>(*m_extensionRegistry, *this);

    m_themeRegistry->RegisterBuiltinThemes();

    return true;
}

void EditorHost::Shutdown()
{
    m_extensionHost->ShutdownAll();

    m_extensionHost.reset();
    m_extensionRegistry.reset();
    m_themeRegistry.reset();
    m_selectionManager.reset();
    m_undoRedoStack.reset();
    m_shortcutManager.reset();
    m_commandPalette.reset();
    m_commandDispatcher.reset();
    m_commandRegistry.reset();
}

// ─── Accessors ────────────────────────────────────────────────────────────────

CommandRegistry&   EditorHost::GetCommandRegistry()   noexcept { return *m_commandRegistry;   }
CommandDispatcher& EditorHost::GetCommandDispatcher() noexcept { return *m_commandDispatcher; }
CommandPalette&    EditorHost::GetCommandPalette()    noexcept { return *m_commandPalette;    }
ShortcutManager&   EditorHost::GetShortcutManager()   noexcept { return *m_shortcutManager;   }
UndoRedoStack&     EditorHost::GetUndoRedoStack()     noexcept { return *m_undoRedoStack;     }
SelectionManager&  EditorHost::GetSelectionManager()  noexcept { return *m_selectionManager;  }
ThemeRegistry&     EditorHost::GetThemeRegistry()     noexcept { return *m_themeRegistry;     }
ExtensionRegistry& EditorHost::GetExtensionRegistry() noexcept { return *m_extensionRegistry; }
ExtensionHost&     EditorHost::GetExtensionHost()     noexcept { return *m_extensionHost;     }

} // namespace SagaEditor
