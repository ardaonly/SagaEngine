/// @file EditorHost.cpp
/// @brief Central service registry for all editor subsystems.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandPalette.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/Persona/PersonaActivator.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Persona/PersonaSettingsBinding.h"
#include "SagaEditor/Profile/EditorProfileActivator.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Profile/EditorProfileSettingsBinding.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"
#include "SagaEditor/Runtime/LocalEditorEngineBridge.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/ExtensionHost.h"

#include <utility>

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

EditorHost::EditorHost()  = default;
EditorHost::~EditorHost() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorHost::Init(std::unique_ptr<IEditorSettingsStore> settingsStore,
                      std::filesystem::path workspaceRoot)
{
    EditorWorkspaceDefinition workspace;
    workspace.root = std::move(workspaceRoot);
    return Init(std::move(settingsStore), std::move(workspace));
}

bool EditorHost::Init(std::unique_ptr<IEditorSettingsStore> settingsStore,
                      EditorWorkspaceDefinition workspace)
{
    m_commandRegistry   = std::make_unique<CommandRegistry>();
    m_commandDispatcher = std::make_unique<CommandDispatcher>(*m_commandRegistry);
    // CommandPalette takes both the registry (to enumerate commands) and
    // the dispatcher (to invoke the chosen command) — order matches the
    // header signature `CommandPalette(CommandRegistry&, CommandDispatcher&)`.
    m_commandPalette    = std::make_unique<CommandPalette>(*m_commandRegistry,
                                                            *m_commandDispatcher);
    m_shortcutManager   = std::make_unique<ShortcutManager>(*m_commandDispatcher);
    m_undoRedoStack     = std::make_unique<UndoRedoStack>();
    m_selectionManager  = std::make_unique<SelectionManager>();
    m_themeRegistry     = std::make_unique<ThemeRegistry>();
    m_personaRegistry   = std::make_unique<PersonaRegistry>();
    m_personaActivator  = std::make_unique<PersonaActivator>();
    m_personaSettingsBinding = std::make_unique<PersonaSettingsBinding>();
    m_editorProfileRegistry = std::make_unique<EditorProfileRegistry>();
    m_editorProfileActivator = std::make_unique<EditorProfileActivator>();
    m_editorProfileSettingsBinding =
        std::make_unique<EditorProfileSettingsBinding>();
    m_settingsStore     = settingsStore ? std::move(settingsStore)
                                        : std::make_unique<MemoryEditorSettingsStore>();
    m_engineBridge      = std::make_unique<LocalEditorEngineBridge>();
    m_customizationCatalog = std::make_unique<EditorCustomizationCatalog>();
    m_extensionRegistry = std::make_unique<ExtensionRegistry>();
    m_extensionHost     = std::make_unique<ExtensionHost>(*m_extensionRegistry, *this);

    m_themeRegistry->RegisterBuiltinThemes();
    m_personaRegistry->RegisterBuiltinPersonas();
    m_editorProfileRegistry->RegisterBuiltinProfiles();
    m_workspaceDefinition = std::move(workspace);
    (void)m_customizationCatalog->Init(m_workspaceDefinition->root);
    for (const EditorProfile& profile :
         m_customizationCatalog->Snapshot().projectProfiles)
    {
        m_editorProfileRegistry->Register(profile);
    }
    for (const UIPersona& persona :
         m_customizationCatalog->Snapshot().projectPersonas)
    {
        m_personaRegistry->Register(persona);
    }
    if (!m_personaSettingsBinding->Init(*m_personaRegistry, *m_settingsStore))
        return false;
    if (!m_editorProfileSettingsBinding->Init(*m_editorProfileRegistry, *m_settingsStore))
        return false;
    if (!m_engineBridge->Init())
        return false;

    return true;
}

void EditorHost::Shutdown()
{
    if (m_extensionHost)
    {
        m_extensionHost->ShutdownAll();
    }

    if (m_personaSettingsBinding)
    {
        m_personaSettingsBinding->Shutdown();
    }
    if (m_editorProfileSettingsBinding)
    {
        m_editorProfileSettingsBinding->Shutdown();
    }
    if (m_customizationCatalog)
    {
        m_customizationCatalog->Shutdown();
    }
    if (m_engineBridge)
    {
        m_engineBridge->Shutdown();
    }

    m_extensionHost.reset();
    m_extensionRegistry.reset();
    m_workspaceDefinition.reset();
    m_customizationCatalog.reset();
    m_engineBridge.reset();
    m_editorProfileSettingsBinding.reset();
    m_editorProfileActivator.reset();
    m_editorProfileRegistry.reset();
    m_personaSettingsBinding.reset();
    m_settingsStore.reset();
    m_personaActivator.reset();
    m_personaRegistry.reset();
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
PersonaRegistry&   EditorHost::GetPersonaRegistry()   noexcept { return *m_personaRegistry;   }
PersonaActivator&  EditorHost::GetPersonaActivator()  noexcept { return *m_personaActivator;  }
EditorProfileRegistry& EditorHost::GetEditorProfileRegistry() noexcept
{
    return *m_editorProfileRegistry;
}
EditorProfileActivator& EditorHost::GetEditorProfileActivator() noexcept
{
    return *m_editorProfileActivator;
}
IEditorSettingsStore& EditorHost::GetSettingsStore()  noexcept { return *m_settingsStore;     }
IEditorEngineBridge& EditorHost::GetEngineBridge() noexcept { return *m_engineBridge; }
EditorCustomizationCatalog& EditorHost::GetCustomizationCatalog() noexcept
{
    return *m_customizationCatalog;
}

const std::optional<EditorWorkspaceDefinition>&
EditorHost::GetWorkspaceDefinition() const noexcept
{
    return m_workspaceDefinition;
}
ExtensionRegistry& EditorHost::GetExtensionRegistry() noexcept { return *m_extensionRegistry; }
ExtensionHost&     EditorHost::GetExtensionHost()     noexcept { return *m_extensionHost;     }

} // namespace SagaEditor
