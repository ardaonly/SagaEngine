// SPDX-License-Identifier: MPL-2.0
/// @file EditorHost.cpp
/// @brief Central service registry for all editor subsystems.

#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandPalette.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Composition/EditorCompositionSession.h"
#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Customization/ShortcutCustomizationSession.h"
#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"
#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditor/Workspaces/UserWorkspace.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Scripting/ScriptProjectionSession.h"
#include "SagaEditor/Scripting/SagaScriptLanguageServiceSession.h"
#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Packages/PackageWorkflowController.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"
#include "SagaEditor/Runtime/LocalEditorEngineBridge.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/ExtensionHost.h"

#include <utility>
#include <vector>

namespace SagaEditor
{
namespace
{

constexpr const char* kCompositionDiagnosticSource = "editor.composition";

[[nodiscard]] EditorDiagnosticSeverity ToEditorSeverity(
    EditorCompositionDiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case EditorCompositionDiagnosticSeverity::Info:
        return EditorDiagnosticSeverity::Info;
    case EditorCompositionDiagnosticSeverity::Warning:
        return EditorDiagnosticSeverity::Warning;
    case EditorCompositionDiagnosticSeverity::Error:
    case EditorCompositionDiagnosticSeverity::Blocker:
        return EditorDiagnosticSeverity::Error;
    }

    return EditorDiagnosticSeverity::Error;
}

[[nodiscard]] std::vector<EditorDiagnostic> ToEditorDiagnostics(
    const std::vector<EditorCompositionDiagnostic>& compositionDiagnostics)
{
    std::vector<EditorDiagnostic> diagnostics;
    diagnostics.reserve(compositionDiagnostics.size());
    for (const EditorCompositionDiagnostic& compositionDiagnostic :
         compositionDiagnostics)
    {
        EditorDiagnostic diagnostic;
        diagnostic.severity = ToEditorSeverity(compositionDiagnostic.severity);
        diagnostic.code = compositionDiagnostic.code;
        diagnostic.message = compositionDiagnostic.message;
        diagnostic.location.resource = compositionDiagnostic.documentPath;
        diagnostic.publishBlocking =
            compositionDiagnostic.severity ==
            EditorCompositionDiagnosticSeverity::Blocker;
        diagnostics.push_back(std::move(diagnostic));
    }

    return diagnostics;
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

EditorHost::EditorHost()  = default;
EditorHost::~EditorHost() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorHost::Init(std::unique_ptr<IEditorSettingsStore> settingsStore,
                      std::filesystem::path workspaceRoot,
                      EditorCompositionStartupConfig composition)
{
    EditorWorkspaceDefinition workspace;
    workspace.root = std::move(workspaceRoot);
    return Init(std::move(settingsStore),
                std::move(workspace),
                std::move(composition));
}

bool EditorHost::Init(std::unique_ptr<IEditorSettingsStore> settingsStore,
                      EditorWorkspaceDefinition workspace,
                      EditorCompositionStartupConfig composition)
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
    m_editorWorkModeService = std::make_unique<EditorWorkModeService>();
    m_userWorkspaceService = std::make_unique<UserWorkspaceService>();
    m_selectionManager  = std::make_unique<SelectionManager>();
    m_documentWorkspace = std::make_unique<EditorDocumentWorkspace>();
    m_scriptAnalysisCoordinator = std::make_unique<ScriptAnalysisCoordinator>();
    m_scriptProjectionSession = std::make_unique<ScriptProjectionSession>();
    m_projectReadinessSession = std::make_unique<ProjectReadinessSession>();
    m_packageStateCatalog = std::make_unique<PackageStateCatalog>();
    m_packageModuleWorkflow = std::make_unique<PackageModuleWorkflow>(
        *m_projectReadinessSession, *m_packageStateCatalog);
    m_themeRegistry     = std::make_unique<ThemeRegistry>();
    m_settingsStore     = settingsStore ? std::move(settingsStore)
                                        : std::make_unique<MemoryEditorSettingsStore>();
    if (!m_editorWorkModeService->Init(*m_settingsStore) ||
        !m_userWorkspaceService->Init(*m_settingsStore,
            m_settingsStore->ConfigurationRoot(), m_editorWorkModeService->Modes()) ||
        !m_userWorkspaceService->ActivateMode(m_editorWorkModeService->Active()))
        return false;
    m_engineBridge      = std::make_unique<LocalEditorEngineBridge>();
    m_editorDiagnosticsService = std::make_unique<EditorDiagnosticsService>();
    m_languageServiceSession = std::make_unique<SagaScriptLanguageServiceSession>(
        *m_documentWorkspace, *m_editorDiagnosticsService);
    m_packageWorkflowController = std::make_unique<PackageWorkflowController>(
        *m_documentWorkspace, *m_projectReadinessSession, *m_packageStateCatalog,
        *m_packageModuleWorkflow, *m_scriptAnalysisCoordinator,
        *m_scriptProjectionSession, *m_editorDiagnosticsService);
    m_editorNotificationCenter = std::make_unique<EditorNotificationCenter>();
    m_customizationCatalog = std::make_unique<EditorCustomizationCatalog>();
    m_shortcutCustomizationSession =
        std::make_unique<ShortcutCustomizationSession>();
    m_workspaceCustomizationSession =
        std::make_unique<WorkspaceCustomizationSession>();
    m_compositionSession = std::make_unique<EditorCompositionSession>();
    m_extensionRegistry = std::make_unique<ExtensionRegistry>();
    m_extensionHost     = std::make_unique<ExtensionHost>(*m_extensionRegistry, *this);

    m_themeRegistry->RegisterBuiltinThemes();
    const std::string preferredTheme = m_settingsStore->GetString(
        AuthoringModeService::ThemeSettingKey, {});
    if (!preferredTheme.empty())
        (void)m_themeRegistry->Apply(preferredTheme);
    m_workspaceDefinition = std::move(workspace);
    (void)m_customizationCatalog->Init(m_workspaceDefinition->root);
    if (!m_engineBridge->Init())
        return false;

    const bool compositionReady = m_compositionSession->Init(composition);
    m_editorDiagnosticsService->ReplaceSource(
        kCompositionDiagnosticSource,
        ToEditorDiagnostics(m_compositionSession->Diagnostics()));

    WorkspaceCustomizationSessionConfig customizationConfig;
    customizationConfig.workspaceRoot = m_workspaceDefinition->root;
    if (m_compositionSession->Snapshot().has_value())
    {
        customizationConfig.snapshot = &*m_compositionSession->Snapshot();
    }
    customizationConfig.diagnosticsService = m_editorDiagnosticsService.get();
    (void)m_workspaceCustomizationSession->Init(
        std::move(customizationConfig));

    ShortcutCustomizationSessionConfig shortcutCustomizationConfig;
    shortcutCustomizationConfig.workspaceRoot = m_workspaceDefinition->root;
    if (m_compositionSession->Snapshot().has_value())
    {
        shortcutCustomizationConfig.snapshot = &*m_compositionSession->Snapshot();
    }
    shortcutCustomizationConfig.diagnosticsService =
        m_editorDiagnosticsService.get();
    (void)m_shortcutCustomizationSession->Init(
        std::move(shortcutCustomizationConfig));

    if (!compositionReady)
        return false;

    return true;
}

void EditorHost::Shutdown()
{
    if (m_extensionHost)
    {
        m_extensionHost->ShutdownAll();
    }

    if (m_customizationCatalog)
    {
        m_customizationCatalog->Shutdown();
    }
    if (m_compositionSession)
    {
        m_compositionSession->Shutdown();
    }
    if (m_workspaceCustomizationSession)
    {
        m_workspaceCustomizationSession->Shutdown();
    }
    if (m_shortcutCustomizationSession)
    {
        m_shortcutCustomizationSession->Shutdown();
    }
    if (m_engineBridge)
    {
        m_engineBridge->Shutdown();
    }

    m_extensionHost.reset();
    m_extensionRegistry.reset();
    m_workspaceDefinition.reset();
    m_shortcutCustomizationSession.reset();
    m_workspaceCustomizationSession.reset();
    m_compositionSession.reset();
    m_customizationCatalog.reset();
    m_editorNotificationCenter.reset();
    m_packageWorkflowController.reset();
    m_languageServiceSession.reset();
    m_editorDiagnosticsService.reset();
    m_engineBridge.reset();
    m_settingsStore.reset();
    m_themeRegistry.reset();
    m_selectionManager.reset();
    m_documentWorkspace.reset();
    m_scriptAnalysisCoordinator.reset();
    m_scriptProjectionSession.reset();
    m_packageModuleWorkflow.reset();
    m_projectReadinessSession.reset();
    m_packageStateCatalog.reset();
    m_undoRedoStack.reset();
    m_userWorkspaceService.reset();
    m_editorWorkModeService.reset();
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
EditorWorkModeService& EditorHost::GetEditorWorkModeService() noexcept
{
    return *m_editorWorkModeService;
}
EditorWorkModeService& EditorHost::GetAuthoringModeService() noexcept
{ return GetEditorWorkModeService(); }
UserWorkspaceService& EditorHost::GetUserWorkspaceService() noexcept
{ return *m_userWorkspaceService; }
SelectionManager&  EditorHost::GetSelectionManager()  noexcept { return *m_selectionManager;  }
EditorDocumentWorkspace& EditorHost::GetDocumentWorkspace() noexcept { return *m_documentWorkspace; }
ScriptAnalysisCoordinator& EditorHost::GetScriptAnalysisCoordinator() noexcept
{ return *m_scriptAnalysisCoordinator; }
ScriptProjectionSession& EditorHost::GetScriptProjectionSession() noexcept
{ return *m_scriptProjectionSession; }
SagaScriptLanguageServiceSession& EditorHost::GetSagaScriptLanguageServiceSession() noexcept
{ return *m_languageServiceSession; }
ProjectReadinessSession& EditorHost::GetProjectReadinessSession() noexcept
{ return *m_projectReadinessSession; }
PackageStateCatalog& EditorHost::GetPackageStateCatalog() noexcept
{ return *m_packageStateCatalog; }
PackageModuleWorkflow& EditorHost::GetPackageModuleWorkflow() noexcept
{ return *m_packageModuleWorkflow; }
PackageWorkflowController& EditorHost::GetPackageWorkflowController() noexcept
{ return *m_packageWorkflowController; }
ThemeRegistry&     EditorHost::GetThemeRegistry()     noexcept { return *m_themeRegistry;     }
IEditorSettingsStore& EditorHost::GetSettingsStore()  noexcept { return *m_settingsStore;     }
IEditorEngineBridge& EditorHost::GetEngineBridge() noexcept { return *m_engineBridge; }
IEditorDiagnosticsService& EditorHost::GetEditorDiagnosticsService() noexcept
{
    return *m_editorDiagnosticsService;
}

EditorNotificationCenter& EditorHost::GetEditorNotificationCenter() noexcept
{
    return *m_editorNotificationCenter;
}

const EditorNotificationCenter&
EditorHost::GetEditorNotificationCenter() const noexcept
{
    return *m_editorNotificationCenter;
}

EditorCustomizationCatalog& EditorHost::GetCustomizationCatalog() noexcept
{
    return *m_customizationCatalog;
}

WorkspaceCustomizationSession&
EditorHost::GetWorkspaceCustomizationSession() noexcept
{
    return *m_workspaceCustomizationSession;
}

const WorkspaceCustomizationSession&
EditorHost::GetWorkspaceCustomizationSession() const noexcept
{
    return *m_workspaceCustomizationSession;
}

ShortcutCustomizationSession&
EditorHost::GetShortcutCustomizationSession() noexcept
{
    return *m_shortcutCustomizationSession;
}

const ShortcutCustomizationSession&
EditorHost::GetShortcutCustomizationSession() const noexcept
{
    return *m_shortcutCustomizationSession;
}

EditorCompositionSession& EditorHost::GetCompositionSession() noexcept
{
    return *m_compositionSession;
}

const EditorCompositionSession& EditorHost::GetCompositionSession() const noexcept
{
    return *m_compositionSession;
}

const std::optional<EditorWorkspaceDefinition>&
EditorHost::GetWorkspaceDefinition() const noexcept
{
    return m_workspaceDefinition;
}

void EditorHost::RefreshWorkspaceCustomizationAvailability(
    std::vector<std::string> registeredPanelIds)
{
    if (!m_workspaceCustomizationSession)
    {
        return;
    }

    EditorCustomizationAvailability availability;
    availability.registeredPanelIds = std::move(registeredPanelIds);
    if (m_compositionSession &&
        m_compositionSession->Snapshot().has_value() &&
        !m_compositionSession->Snapshot()->workspaces.empty())
    {
        availability.activeWorkspaceId =
            m_compositionSession->Snapshot()->workspaces.front().id;
    }

    for (const CommandDescriptor* command : m_commandRegistry->GetAll())
    {
        if (command == nullptr)
        {
            continue;
        }
        availability.availableActionIds.push_back(command->id);
        availability.shortcutAssignableActionIds.push_back(command->id);
    }

    EditorCustomizationAvailability shortcutAvailability = availability;

    m_workspaceCustomizationSession->RefreshAvailability(std::move(availability));
    if (m_shortcutCustomizationSession)
    {
        m_shortcutCustomizationSession->RefreshAvailability(
            std::move(shortcutAvailability));
    }
}
ExtensionRegistry& EditorHost::GetExtensionRegistry() noexcept { return *m_extensionRegistry; }
ExtensionHost&     EditorHost::GetExtensionHost()     noexcept { return *m_extensionHost;     }

} // namespace SagaEditor
