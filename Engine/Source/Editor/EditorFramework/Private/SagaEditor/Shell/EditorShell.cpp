// SPDX-License-Identifier: MPL-2.0
/// @file EditorShell.cpp
/// @brief Main editor shell implementation.

#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Composition/EditorCompositionSession.h"
#include "SagaEditor/Composition/EditorCompositionShellAdapter.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Extensions/ExtensionHost.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Packages/PackageAuthoring.h"
#include "SagaEditor/Packages/PackageWorkflowController.h"
#include "SagaEditor/Settings/IEditorSettingsStore.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditor/Play/RuntimeProcessController.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Panels/IPanel.h"

#include <unordered_set>
#include <utility>
#include <vector>

// Default Qt panels (concrete types live in the Qt backend)
#include "SagaEditor/Panels/HierarchyPanel.h"
#include "SagaEditor/Panels/InspectorPanel.h"
#include "SagaEditor/Panels/AssetBrowserPanel.h"
#include "SagaEditor/Panels/ProblemsPanel.h"
#include "SagaEditor/Panels/PackagesPanel.h"
#include "SagaEditor/Panels/CodeBoxPanel.h"
#include "SagaEditor/Panels/WorldViewportPanel.h"

#include <algorithm>
#include <charconv>
#include <chrono>

namespace SagaEditor
{

namespace
{

constexpr const char* kCompositionShellDiagnosticSource =
    "editor.composition.shell";
constexpr const char* kModeLayoutPrefix = "Editor/Layout/v2/";

std::string EncodeHex(const std::vector<std::uint8_t>& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        result[index * 2] = digits[bytes[index] >> 4U];
        result[index * 2 + 1] = digits[bytes[index] & 0x0fU];
    }
    return result;
}

std::optional<std::vector<std::uint8_t>> DecodeHex(const std::string& text)
{
    if (text.size() % 2 != 0) return std::nullopt;
    std::vector<std::uint8_t> result;
    result.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
    {
        unsigned int value = 0;
        const auto parsed = std::from_chars(text.data() + index, text.data() + index + 2,
                                            value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + index + 2)
            return std::nullopt;
        result.push_back(static_cast<std::uint8_t>(value));
    }
    return result;
}

[[nodiscard]] bool SameDiagnosticIdentity(const EditorDiagnostic& lhs,
                                          const EditorDiagnostic& rhs)
{
    return lhs.severity == rhs.severity &&
           lhs.code == rhs.code &&
           lhs.message == rhs.message &&
           lhs.location.resource == rhs.location.resource &&
           lhs.location.line == rhs.location.line &&
           lhs.location.column == rhs.location.column;
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

EditorShell::EditorShell()  = default;
EditorShell::~EditorShell() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorShell::Init(EditorHost&                    host,
                       std::unique_ptr<IUIMainWindow> window,
                       const EditorShellConfig&       cfg)
{
    m_host = &host;
    m_canReturnHome = static_cast<bool>(cfg.returnHome);
    m_workspaceChanged = cfg.workspaceChanged;
    m_window = std::move(window);
    if (!m_window)
        return false;

    m_window->SetTitle(cfg.windowTitle);
    m_window->SetSize(cfg.windowWidth, cfg.windowHeight);

    m_projectWorkflowController =
        std::make_unique<EditorProjectWorkflowController>(
            host.GetDocumentWorkspace(), cfg.projectWorkflowPrompts);
    m_window->SetOnClose([this]
    {
        return m_projectWorkflowController->CanClose() ==
            ProjectWorkflowOutcome::Completed;
    });
    m_window->SetOnCommand(
        [&host](const std::string& commandId)
        {
            (void)host.GetCommandDispatcher().Dispatch(commandId);
        });
    m_window->SetOnWorkflowStatePoll([this]
    {
        SyncWorkflowCommandState();
    });

    // RegisterShellCommands takes (registry, undoStack); the older one-arg
    // overload that took the whole host doesn't exist. Pull both services
    // off the host directly.
    RegisterShellCommands(host.GetCommandRegistry(),
                          host.GetUndoRedoStack());
    m_runtimeProcessController = std::make_unique<RuntimeProcessController>();
    host.GetCommandRegistry().Register({"saga.command.world.play", "Play", "World",
        [this, &host, runtimeExecutable = cfg.runtimeExecutable]
        {
            if (!host.GetProjectReadinessSession().CanPlayOrBuild())
            {
                m_window->SetStatusMessage(
                    "Play is blocked by current project readiness.");
                return;
            }
            if (runtimeExecutable.empty() || !std::filesystem::exists(runtimeExecutable))
            {
                m_window->SetStatusMessage("SagaRuntime executable is unavailable for Play.");
                return;
            }
            const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
            RuntimeProcessLaunchRequest request;
            request.playSessionId = "editor-play-" + std::to_string(ticks);
            request.runtimeExecutable = runtimeExecutable;
            const bool started = m_runtimeProcessController->Start(
                host.GetDocumentWorkspace(), std::move(request));
            m_window->SetStatusMessage(started ?
                "Preparing saved project source for separate SagaRuntime Play..." :
                "Play is already active or the request is invalid.");
        }});
    host.GetCommandRegistry().Register({"saga.command.world.stop", "Stop", "World",
        [this]
        {
            m_runtimeProcessController->Stop();
            m_window->SetStatusMessage("Stop requested for the active SagaRuntime session.");
        }});
    host.GetCommandRegistry().Register({"saga.command.file.save", "Save All", "File",
        [this, &host]
        {
            const bool saved = host.GetDocumentWorkspace().SaveAll();
            m_window->SetStatusMessage(saved ? "Saved canonical source documents." :
                "Save failed; inspect document diagnostics.");
        }});
    host.GetCommandRegistry().Register({"saga.command.file.open_project", "Open Project", "File",
        [this]
        {
            const auto outcome =
                m_projectWorkflowController->OpenProjectPrompted();
            if (outcome == ProjectWorkflowOutcome::Completed)
            {
                m_window->SetStatusMessage("Opened canonical Saga project.");
                if (auto* panel = dynamic_cast<PackagesPanel*>(
                        FindPanel("saga.panel.packages")))
                    panel->OpenOrRefresh();
                if (m_workspaceChanged)
                    m_workspaceChanged();
            }
            else if (outcome == ProjectWorkflowOutcome::Failed)
                m_window->SetStatusMessage(
                    "Project open failed validation; the previous workspace is unchanged.");
        }});
    host.GetCommandRegistry().Register({"saga.command.file.new_project", "New Project", "File",
        [this]
        {
            const auto outcome =
                m_projectWorkflowController->CreateProjectPrompted();
            if (outcome == ProjectWorkflowOutcome::Completed)
            {
                m_window->SetStatusMessage(
                    "Created and opened Starter Arena project.");
                if (auto* panel = dynamic_cast<PackagesPanel*>(
                        FindPanel("saga.panel.packages")))
                    panel->OpenOrRefresh();
                if (m_workspaceChanged)
                    m_workspaceChanged();
            }
            else if (outcome == ProjectWorkflowOutcome::Failed)
                m_window->SetStatusMessage(
                    "Project creation failed transactionally; destination was not opened.");
        }});
    host.GetCommandRegistry().Register({"saga.command.file.exit", "Exit", "File",
        [this] { m_window->Close(); }});
    if (cfg.returnHome)
    {
        host.GetCommandRegistry().Register({"saga.command.file.close_project",
            "Close Project", "File", [this, returnHome = cfg.returnHome]
            {
                if (m_projectWorkflowController->CanClose() !=
                    ProjectWorkflowOutcome::Completed)
                    return;
                if (m_runtimeProcessController)
                    m_runtimeProcessController->Shutdown();
                returnHome();
            }});
    }
    WireNotificationSink();
    host.GetCommandRegistry().Register({
        "saga.command.package.enable_module", "Enable Package Module…",
        "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages")))
                panel->EnableSelected();
        }});
    host.GetCommandRegistry().Register({
        "saga.command.package.disable_module", "Disable Package Module…",
        "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages")))
                panel->DisableSelected();
        }});
    host.GetCommandRegistry().Register({
        "saga.command.package.retry_refresh", "Retry Package Refresh",
        "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages")))
                panel->RetryRefresh();
        }});
    host.GetCommandRegistry().Register({
        "saga.command.package.recover_transaction", "Recover Package Transaction…",
        "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages")))
                panel->InspectRecovery();
        }});
    host.GetCommandRegistry().Register({
        "saga.command.package.undo", "Package Undo…", "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages"))) panel->Undo();
        }});
    host.GetCommandRegistry().Register({
        "saga.command.package.redo", "Package Redo…", "Packages", [this]
        {
            (void)m_host->GetCommandDispatcher().Dispatch(
                "saga.command.view.packages");
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages"))) panel->Redo();
        }});
    RegisterAuthoringModeCommands();

    BuildDefaultLayout();
    (void)BuildCompositionLayout();
    m_window->ApplyShellLayout(m_layout);
    (void)ApplyCompositionShortcuts();

    if (cfg.registerDefaultPanels)
    {
        RegisterDefaultPanels();
        if (host.GetDocumentWorkspace().IsOpen())
            if (auto* panel = dynamic_cast<PackagesPanel*>(
                    FindPanel("saga.panel.packages")))
                panel->OpenOrRefresh();
    }
    SyncWorkflowCommandState();
    (void)ApplyCompositionPanelVisibility();
    auto& authoringModes = host.GetAuthoringModeService();
    authoringModes.SetOnChanged([this](const AuthoringMode& mode)
    {
        SaveAuthoringModeLayout();
        ApplyAuthoringMode(mode, true);
    });
    ApplyAuthoringMode(authoringModes.Active(), true);
    if (cfg.showOnInit)
    {
        if (cfg.maximized)
            m_window->ShowMaximized();
        else
            m_window->Show();
    }

    host.GetExtensionHost().SetShell(*this);
    return true;
}

void EditorShell::Shutdown()
{
    if (m_host && m_window)
        m_host->GetExtensionHost().ShutdownAll();
    if (m_window)
        m_window->SetOnWorkflowStatePoll({});

    m_projectWorkflowController.reset();
    if (m_runtimeProcessController)
    {
        m_runtimeProcessController->Shutdown();
        m_runtimeProcessController.reset();
    }
    if (m_host && m_window)
    {
        SaveAuthoringModeLayout();
        m_host->GetSettingsStore().Flush();
    }
    if (m_host && m_notificationSubscription !=
                      kInvalidEditorNotificationSubscription)
    {
        m_host->GetEditorNotificationCenter().Unsubscribe(
            m_notificationSubscription);
        m_notificationSubscription = kInvalidEditorNotificationSubscription;
    }

    for (auto& panel : m_panels)
        panel->OnShutdown();

    m_panels.clear();
    m_window.reset();
}

// ─── Panel Management ─────────────────────────────────────────────────────────

void EditorShell::RegisterPanel(std::unique_ptr<IPanel> panel, UIDockArea area)
{
    panel->OnInit();
    const std::string panelId = panel->GetPanelId();
    const std::string panelTitle = panel->GetTitle();
    m_window->DockPanel(panel->Widget(),
                        panelId,
                        panelTitle,
                        ResolveCompositionDockArea(panelId, area));
    m_panels.push_back(std::move(panel));

    const auto viewCommandForPanel = [](const std::string& id) -> std::string
    {
        if (id == "saga.panel.hierarchy") return "saga.command.view.hierarchy";
        if (id == "saga.panel.inspector") return "saga.command.view.inspector";
        if (id == "saga.panel.assetbrowser") return "saga.command.view.project_sources";
        if (id == "saga.panel.code_box") return "saga.command.view.code_box";
        if (id == "saga.panel.problems") return "saga.command.view.problems";
        if (id == "saga.panel.packages") return "saga.command.view.packages";
        return {};
    };
    const std::string viewCommand = viewCommandForPanel(panelId);
    if (!viewCommand.empty())
    {
        m_host->GetCommandRegistry().Register({
            viewCommand,
            panelTitle,
            "View",
            [this, panelId]()
            {
                m_window->SetPanelVisible(panelId, true);
                m_window->FocusPanel(panelId);
            },
            true
        });
    }

    if (const std::optional<bool> compositionVisible =
            GetCompositionPanelVisibility(panelId))
    {
        m_window->SetPanelVisible(panelId, *compositionVisible);
    }
    (void)ApplyCompositionPanelVisibility();
    m_host->RefreshWorkspaceCustomizationAvailability(GetRegisteredPanelIds());
}

IPanel* EditorShell::FindPanel(const std::string& panelId) const
{
    for (const auto& p : m_panels)
    {
        if (p->GetPanelId() == panelId)
            return p.get();
    }
    return nullptr;
}

std::vector<std::string> EditorShell::GetRegisteredPanelTitles() const
{
    std::vector<std::string> titles;
    titles.reserve(m_panels.size());
    for (const auto& panel : m_panels)
    {
        titles.push_back(panel->GetTitle());
    }
    return titles;
}

// ─── Shell Chrome ─────────────────────────────────────────────────────────────

void EditorShell::SetShellLayout(ShellLayout layout)
{
    m_layout = std::move(layout);
    m_window->ApplyShellLayout(m_layout);
}

const ShellLayout& EditorShell::GetShellLayout() const noexcept
{
    return m_layout;
}

const std::string& EditorShell::GetActiveLayoutPresetId() const noexcept
{
    return m_activeLayoutPresetId;
}

const std::vector<std::string>& EditorShell::GetActiveToolbarCommands() const noexcept
{
    return m_activeToolbarCommands;
}

const std::vector<std::string>& EditorShell::GetActiveToolCommands() const noexcept
{
    return m_activeToolCommands;
}

// ─── Accessors ────────────────────────────────────────────────────────────────

IUIMainWindow& EditorShell::GetMainWindow() noexcept { return *m_window; }
EditorHost&    EditorShell::GetHost()       noexcept { return *m_host; }

bool EditorShell::CanCloseWorkspace()
{
    return !m_projectWorkflowController ||
        m_projectWorkflowController->CanClose() == ProjectWorkflowOutcome::Completed;
}

// ─── Internals ────────────────────────────────────────────────────────────────

void EditorShell::RegisterDefaultPanels()
{
    auto hierarchy = std::make_unique<HierarchyPanel>();
    hierarchy->SetDocumentWorkspace(&m_host->GetDocumentWorkspace());
    hierarchy->SetSelectionManager(&m_host->GetSelectionManager());
    RegisterPanel(std::move(hierarchy), UIDockArea::Left);
    auto viewport = std::make_unique<WorldViewportPanel>();
    viewport->SetDocumentWorkspace(&m_host->GetDocumentWorkspace());
    viewport->SetSelectionManager(&m_host->GetSelectionManager());
    RegisterPanel(std::move(viewport), UIDockArea::Center);
    auto inspector = std::make_unique<InspectorPanel>();
    inspector->SetDocumentWorkspace(&m_host->GetDocumentWorkspace());
    inspector->SetSelectionManager(&m_host->GetSelectionManager());
    inspector->SetUndoRedoStack(&m_host->GetUndoRedoStack());
    RegisterPanel(std::move(inspector), UIDockArea::Right);
    auto sources = std::make_unique<AssetBrowserPanel>();
    sources->SetDocumentWorkspace(&m_host->GetDocumentWorkspace());
    RegisterPanel(std::move(sources), UIDockArea::Bottom);
    auto codeBox = std::make_unique<CodeBoxPanel>();
    codeBox->SetDocumentWorkspace(&m_host->GetDocumentWorkspace());
    codeBox->SetUndoRedoStack(&m_host->GetUndoRedoStack());
    codeBox->SetCommandDispatcher(&m_host->GetCommandDispatcher());
    codeBox->SetAnalysisCoordinator(&m_host->GetScriptAnalysisCoordinator());
    codeBox->SetProjectionSession(&m_host->GetScriptProjectionSession());
    auto* codeBoxPanel = codeBox.get();
    m_host->GetCommandRegistry().Register({"saga.command.script.analyze", "Analyze C#", "Script",
        [codeBoxPanel] { codeBoxPanel->Analyze(); }});
    m_host->GetCommandRegistry().Register({"saga.command.script.cancel", "Cancel Analysis", "Script",
        [codeBoxPanel] { codeBoxPanel->CancelAnalysis(); }});
    RegisterPanel(std::move(codeBox), UIDockArea::Bottom);

    auto problems = std::make_unique<ProblemsPanel>();
    problems->SetDiagnosticsService(&m_host->GetEditorDiagnosticsService());
    RegisterPanel(std::move(problems), UIDockArea::Bottom);
    auto packages = std::make_unique<PackagesPanel>();
    packages->SetCatalog(&m_host->GetPackageStateCatalog());
    packages->SetReadiness(&m_host->GetProjectReadinessSession());
    packages->SetWorkflow(&m_host->GetPackageModuleWorkflow());
    packages->SetController(&m_host->GetPackageWorkflowController());
    packages->SetCommandDispatcher(&m_host->GetCommandDispatcher());
    RegisterPanel(std::move(packages), UIDockArea::Bottom);
}

void EditorShell::WireNotificationSink()
{
    m_notificationSubscription =
        m_host->GetEditorNotificationCenter().Subscribe(
            [this](const EditorNotification& notification)
            {
                if (m_window != nullptr && !notification.message.empty())
                {
                    m_window->SetStatusMessage(notification.message);
                }
            });
}

void EditorShell::SyncWorkflowCommandState()
{
    if (!m_host || !m_window)
        return;

    auto& workspace = m_host->GetDocumentWorkspace();
    const bool projectOpen = workspace.IsOpen();
    const bool dirty = projectOpen && !workspace.DirtyDocumentIds().empty();
    m_host->GetProjectReadinessSession().SetDirty(dirty);
    const RuntimePlayState runtimeState = m_runtimeProcessController
        ? m_runtimeProcessController->Snapshot().state
        : RuntimePlayState::Idle;
    const bool playTransition =
        runtimeState == RuntimePlayState::Preparing ||
        runtimeState == RuntimePlayState::Starting ||
        runtimeState == RuntimePlayState::Stopping;
    const bool playActive =
        runtimeState == RuntimePlayState::Preparing ||
        runtimeState == RuntimePlayState::Starting ||
        runtimeState == RuntimePlayState::Running ||
        runtimeState == RuntimePlayState::Stopping;

    const auto setEnabled = [this](const std::string& id, bool enabled)
    {
        m_host->GetCommandRegistry().SetEnabled(id, enabled);
        m_window->SetCommandEnabled(id, enabled);
    };
    setEnabled("saga.command.file.save", dirty);
    setEnabled("saga.command.edit.undo", m_host->GetUndoRedoStack().CanUndo());
    setEnabled("saga.command.edit.redo", m_host->GetUndoRedoStack().CanRedo());
    setEnabled("saga.command.world.play", projectOpen &&
        m_host->GetProjectReadinessSession().CanPlayOrBuild() && !playActive);
    setEnabled("saga.command.world.stop",
        runtimeState == RuntimePlayState::Preparing ||
        runtimeState == RuntimePlayState::Starting ||
        runtimeState == RuntimePlayState::Running);
    for (const AuthoringMode& mode : m_host->GetAuthoringModeService().Modes())
        setEnabled("saga.command.authoring." + mode.id, !playTransition);
    setEnabled("saga.command.authoring.reset_layout", !playTransition);
    const auto package = m_host->GetPackageWorkflowController().Snapshot();
    const bool packageReady = projectOpen && !package.active &&
        package.state == PackageWorkflowControllerState::Ready;
    setEnabled("saga.command.package.enable_module", packageReady);
    setEnabled("saga.command.package.disable_module", packageReady);
    setEnabled("saga.command.package.retry_refresh", package.canRetry);
    setEnabled("saga.command.package.recover_transaction",
        package.state == PackageWorkflowControllerState::RecoveryRequired);
    setEnabled("saga.command.package.undo", package.canUndo);
    setEnabled("saga.command.package.redo", package.canRedo);
    if (auto* packagePanel = dynamic_cast<PackagesPanel*>(
            FindPanel("saga.panel.packages")))
        packagePanel->Refresh();
}

void EditorShell::RegisterAuthoringModeCommands()
{
    auto& modes = m_host->GetAuthoringModeService();
    for (const AuthoringMode& mode : modes.Modes())
    {
        m_host->GetCommandRegistry().Register({
            "saga.command.authoring." + mode.id, mode.displayName, "Authoring",
            [this, id = mode.id]
            {
                const RuntimePlayState state = m_runtimeProcessController
                    ? m_runtimeProcessController->Snapshot().state
                    : RuntimePlayState::Idle;
                if (state == RuntimePlayState::Preparing ||
                    state == RuntimePlayState::Starting ||
                    state == RuntimePlayState::Stopping)
                    return;
                (void)m_host->GetAuthoringModeService().SetActive(id);
            }});
    }
    m_host->GetCommandRegistry().Register({
        "saga.command.authoring.reset_layout", "Reset Workspace Layout", "Authoring",
        [this]
        {
            m_host->GetSettingsStore().Remove(
                std::string(kModeLayoutPrefix) + m_activeAuthoringModeId);
            m_window->ResetWorkspaceLayout();
            ApplyAuthoringMode(m_host->GetAuthoringModeService().Active(), false);
        }});
}

void EditorShell::SaveAuthoringModeLayout()
{
    if (!m_host || !m_window || m_activeAuthoringModeId.empty())
        return;
    m_host->GetSettingsStore().SetString(
        std::string(kModeLayoutPrefix) + m_activeAuthoringModeId,
        EncodeHex(m_window->SaveState()));
    m_host->GetSettingsStore().Flush();
}

void EditorShell::ApplyAuthoringMode(const AuthoringMode& mode, bool restoreLayout)
{
    m_activeAuthoringModeId = mode.id;
    const std::unordered_set<std::string> defaultVisible(
        mode.defaultVisiblePanels.begin(), mode.defaultVisiblePanels.end());
    bool restoredSavedLayout = false;
    if (restoreLayout)
    {
        const std::string saved = m_host->GetSettingsStore().GetString(
            std::string(kModeLayoutPrefix) + mode.id, {});
        const auto decoded = DecodeHex(saved);
        if (!saved.empty() && decoded)
            restoredSavedLayout = m_window->RestoreState(*decoded);
        if (!saved.empty() && !restoredSavedLayout)
        {
            m_host->GetSettingsStore().Remove(
                std::string(kModeLayoutPrefix) + mode.id);
        }
        if (!restoredSavedLayout)
            m_window->ResetWorkspaceLayout();
    }
    if (!restoredSavedLayout)
    {
        for (const auto& panel : m_panels)
            m_window->SetPanelVisible(panel->GetPanelId(),
                defaultVisible.contains(panel->GetPanelId()));
    }
    for (const AuthoringMode& candidate : m_host->GetAuthoringModeService().Modes())
        m_window->SetCommandChecked("saga.command.authoring." + candidate.id,
                                    candidate.id == mode.id);
    m_window->SetStatusMessage("Authoring Mode: " + mode.displayName);
}

bool EditorShell::BuildCompositionLayout()
{
    const ResolvedEditorCompositionSnapshot* snapshot =
        GetUsableCompositionSnapshot();
    if (snapshot == nullptr)
    {
        return false;
    }

    EditorCompositionShellAdapter adapter;
    EditorCompositionShellLayoutResult result =
        adapter.BuildShellLayout(*snapshot,
                                 m_host->GetCommandRegistry(),
                                 m_layout);
    if (result.usedComposition)
    {
        m_layout = std::move(result.layout);
        m_activeToolbarCommands.clear();
        for (const MenuItemDescriptor& item : m_layout.mainToolbarItems)
        {
            if (!item.commandId.empty())
            {
                m_activeToolbarCommands.push_back(item.commandId);
            }
        }
    }
    MergeCompositionShellDiagnostics(std::move(result.diagnostics));
    return result.usedComposition;
}

bool EditorShell::ApplyCompositionShortcuts()
{
    const ResolvedEditorCompositionSnapshot* snapshot =
        GetUsableCompositionSnapshot();
    if (snapshot == nullptr)
    {
        return false;
    }

    EditorCompositionShellAdapter adapter;
    MergeCompositionShellDiagnostics(
        adapter.ApplyShortcuts(*snapshot,
                               m_host->GetCommandRegistry(),
                               m_host->GetShortcutManager()));
    return true;
}

bool EditorShell::ApplyCompositionPanelVisibility()
{
    const ResolvedEditorCompositionSnapshot* snapshot =
        GetUsableCompositionSnapshot();
    if (snapshot == nullptr)
    {
        return false;
    }

    m_compositionShellDiagnostics.erase(
        std::remove_if(m_compositionShellDiagnostics.begin(),
                       m_compositionShellDiagnostics.end(),
                       [](const EditorDiagnostic& diagnostic)
                       {
                           return diagnostic.code ==
                                  "CompositionShell.MissingPanelImplementation";
                       }),
        m_compositionShellDiagnostics.end());

    EditorCompositionShellAdapter adapter;
    MergeCompositionShellDiagnostics(
        adapter.ValidateRegisteredPanels(*snapshot, GetRegisteredPanelIds()));

    for (const auto& panel : m_panels)
    {
        const std::optional<bool> visible =
            GetCompositionPanelVisibility(panel->GetPanelId());
        m_window->SetPanelVisible(panel->GetPanelId(),
                                  visible.value_or(false));
    }
    return true;
}

void EditorShell::MergeCompositionShellDiagnostics(
    std::vector<EditorDiagnostic> diagnostics)
{
    for (EditorDiagnostic& diagnostic : diagnostics)
    {
        auto it = std::find_if(m_compositionShellDiagnostics.begin(),
                               m_compositionShellDiagnostics.end(),
                               [&diagnostic](const EditorDiagnostic& existing)
                               {
                                   return SameDiagnosticIdentity(existing,
                                                                 diagnostic);
                               });
        if (it == m_compositionShellDiagnostics.end())
        {
            m_compositionShellDiagnostics.push_back(std::move(diagnostic));
        }
    }

    m_host->GetEditorDiagnosticsService().ReplaceSource(
        kCompositionShellDiagnosticSource,
        m_compositionShellDiagnostics);
}

const ResolvedEditorCompositionSnapshot*
EditorShell::GetUsableCompositionSnapshot() const
{
    if (m_host == nullptr ||
        !m_host->GetCompositionSession().IsUsable() ||
        !m_host->GetCompositionSession().Snapshot().has_value())
    {
        return nullptr;
    }

    return &*m_host->GetCompositionSession().Snapshot();
}

std::vector<std::string> EditorShell::GetRegisteredPanelIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_panels.size());
    for (const auto& panel : m_panels)
    {
        ids.push_back(panel->GetPanelId());
    }
    return ids;
}

std::optional<bool> EditorShell::GetCompositionPanelVisibility(
    const std::string& panelId) const
{
    const ResolvedEditorCompositionSnapshot* snapshot =
        GetUsableCompositionSnapshot();
    if (snapshot == nullptr)
    {
        return std::nullopt;
    }

    auto it = std::find_if(snapshot->panels.begin(),
                           snapshot->panels.end(),
                           [&panelId](const ResolvedPanelDefinition& panel)
                           {
                               return panel.definition.id == panelId;
                           });
    if (it == snapshot->panels.end())
    {
        return false;
    }

    return it->visible;
}

UIDockArea EditorShell::ResolveCompositionDockArea(const std::string& panelId,
                                                   UIDockArea fallback) const
{
    const ResolvedEditorCompositionSnapshot* snapshot =
        GetUsableCompositionSnapshot();
    if (snapshot == nullptr)
    {
        return fallback;
    }

    auto it = std::find_if(snapshot->panels.begin(),
                           snapshot->panels.end(),
                           [&panelId](const ResolvedPanelDefinition& panel)
                           {
                               return panel.definition.id == panelId;
                           });
    if (it == snapshot->panels.end())
    {
        return fallback;
    }

    if (it->placement == "left")
        return UIDockArea::Left;
    if (it->placement == "right")
        return UIDockArea::Right;
    if (it->placement == "top")
        return UIDockArea::Top;
    if (it->placement == "bottom")
        return UIDockArea::Bottom;
    if (it->placement == "center")
        return UIDockArea::Center;
    if (it->placement == "floating")
        return UIDockArea::Floating;
    return fallback;
}

void EditorShell::BuildDefaultLayout()
{
    // `MenuDescriptor`/`MenuItemDescriptor` both come from `Shell/ShellLayout.h`.
    // Positional initialisers below match the struct layouts:
    //   MenuDescriptor      { title,    items }
    //   MenuItemDescriptor  { label, commandId, shortcut, separator }
    // A separator entry sets `separator = true` and leaves label/cmd/shortcut empty.

    // ── File menu ─────────────────────────────────────────────────────────────
    MenuDescriptor file;
    file.title = "File";
    file.items = {
        { "New Project…",    "saga.command.file.new_project",   "Ctrl+N",       false },
        { "Open Project…",   "saga.command.file.open_project",  "Ctrl+O",       false },
        { "Save All",        "saga.command.file.save",          "Ctrl+S",       false },
    };
    if (m_canReturnHome)
        file.items.push_back(
            { "Close Project", "saga.command.file.close_project", "", false });
    file.items.push_back({ "", "", "", true });
    file.items.push_back(
        { "Exit", "saga.command.file.exit", "", false });

    // ── Edit menu ─────────────────────────────────────────────────────────────
    MenuDescriptor edit;
    edit.title = "Edit";
    edit.items = {
        { "Undo",            "saga.command.edit.undo",          "Ctrl+Z",       false },
        { "Redo",            "saga.command.edit.redo",          "Ctrl+Shift+Z", false },
    };

    // ── View menu ─────────────────────────────────────────────────────────────
    MenuDescriptor view;
    view.title = "View";
    view.items = {
        { "Hierarchy",       "saga.command.view.hierarchy",     "",             false },
        { "Inspector",       "saga.command.view.inspector",     "",             false },
        { "Project Sources", "saga.command.view.project_sources","",            false },
        { "Code Box",        "saga.command.view.code_box",      "",             false },
        { "Problems",        "saga.command.view.problems",      "",             false },
        { "Packages",        "saga.command.view.packages",      "",             false },
    };
    MenuDescriptor authoring;
    authoring.title = "Authoring";
    authoring.items = {
        { "Level 1", "saga.command.authoring.level1", "", false },
        { "Level 2", "saga.command.authoring.level2", "", false },
        { "Level 3", "saga.command.authoring.level3", "", false },
        { "", "", "", true },
        { "Reset Workspace Layout", "saga.command.authoring.reset_layout", "", false },
    };
    m_layout.menus = { file, edit, view, authoring };
    m_layout.mainToolbarItems = {
        { "Play", "saga.command.world.play", "", false },
        { "Stop", "saga.command.world.stop", "", false },
    };
}

} // namespace SagaEditor
