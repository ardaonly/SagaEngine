/// @file EditorShell.cpp
/// @brief Main editor shell implementation.

#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Composition/EditorCompositionSession.h"
#include "SagaEditor/Composition/EditorCompositionShellAdapter.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Commands/ShortcutManager.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"
#include "SagaEditor/Persona/PersonaActivator.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Persona/UIPersona.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileActivator.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/UI/IUIFactory.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Panels/CustomizeWorkspacePanel.h"
#include "SagaEditor/Panels/ProductionDashboardPanel.h"
#include "SagaEditor/Panels/ShortcutPreferencesPanel.h"

#include <unordered_set>
#include <utility>
#include <vector>

// Default Qt panels (concrete types live in the Qt backend)
#include "SagaEditor/Panels/HierarchyPanel.h"
#include "SagaEditor/Panels/InspectorPanel.h"
#include "SagaEditor/Panels/ConsolePanel.h"
#include "SagaEditor/Panels/AssetBrowserPanel.h"
#include "SagaEditor/Panels/ProblemsPanel.h"
#include "SagaEditor/Panels/WorldViewportPanel.h"

#include <algorithm>

namespace SagaEditor
{

namespace
{

constexpr const char* kCompositionShellDiagnosticSource =
    "editor.composition.shell";

[[nodiscard]] std::string ProfileCommandId(const std::string& profileId)
{
    constexpr const char* prefix = "saga.profile.";
    if (profileId.rfind(prefix, 0) == 0)
    {
        return "saga.command.profile." + profileId.substr(std::char_traits<char>::length(prefix));
    }
    return "saga.command.profile." + profileId;
}

[[nodiscard]] std::string ProfileTitleSuffix(const EditorProfile& profile)
{
    return " - " + profile.displayName;
}

[[nodiscard]] std::unordered_set<std::string> ProfileToolCommandUniverse()
{
    return {
        "saga.command.edit.mode.translate",
        "saga.command.edit.mode.rotate",
        "saga.command.edit.mode.scale",
        "saga.command.build",
        "saga.command.world.step",
        "saga.command.view.graph",
        "saga.command.view.diagnostics",
    };
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

bool EditorShell::Init(EditorHost&              host,
                       IUIFactory&              factory,
                       const EditorShellConfig& cfg)
{
    return Init(host,
                factory.CreateMainWindow(cfg.windowTitle,
                                         cfg.windowWidth,
                                         cfg.windowHeight),
                cfg);
}

bool EditorShell::Init(EditorHost&                    host,
                       std::unique_ptr<IUIMainWindow> window,
                       const EditorShellConfig&       cfg)
{
    m_host = &host;
    m_baseWindowTitle = cfg.windowTitle;

    m_window = std::move(window);
    if (!m_window)
        return false;

    m_window->SetTitle(cfg.windowTitle);
    m_window->SetSize(cfg.windowWidth, cfg.windowHeight);

    // Wire close → quit the event loop.
    m_window->SetOnClose([]{});
    m_window->SetOnCommand(
        [&host](const std::string& commandId)
        {
            (void)host.GetCommandDispatcher().Dispatch(commandId);
        });

    // RegisterShellCommands takes (registry, undoStack); the older one-arg
    // overload that took the whole host doesn't exist. Pull both services
    // off the host directly.
    RegisterShellCommands(host.GetCommandRegistry(),
                          host.GetUndoRedoStack());
    WireNotificationSink();
    RegisterPersonaCommands();
    RegisterProfileCommands();

    BuildDefaultLayout();
    (void)BuildCompositionLayout();
    m_window->ApplyShellLayout(m_layout);
    (void)ApplyCompositionShortcuts();

    WirePersonaSinks();
    WireProfileSinks();
    if (cfg.registerDefaultPanels)
    {
        RegisterDefaultPanels();
    }
    if (cfg.applyActivePersona)
    {
        ApplyActivePersona();
    }
    ApplyActiveProfile();
    (void)ApplyCompositionPanelVisibility();

    if (cfg.showOnInit)
    {
        if (cfg.maximized)
            m_window->ShowMaximized();
        else
            m_window->Show();
    }

    return true;
}

void EditorShell::Shutdown()
{
    if (m_host && m_personaSubscription != kInvalidPersonaSubscription)
    {
        m_host->GetPersonaRegistry().RemovePersonaChangedCallback(m_personaSubscription);
        m_personaSubscription = kInvalidPersonaSubscription;
    }
    if (m_host && m_profileSubscription != kInvalidEditorProfileSubscription)
    {
        m_host->GetEditorProfileRegistry().RemoveProfileChangedCallback(m_profileSubscription);
        m_profileSubscription = kInvalidEditorProfileSubscription;
    }
    if (m_host && m_notificationSubscription !=
                      kInvalidEditorNotificationSubscription)
    {
        m_host->GetEditorNotificationCenter().Unsubscribe(
            m_notificationSubscription);
        m_notificationSubscription = kInvalidEditorNotificationSubscription;
    }

    if (m_host)
    {
        m_host->GetPersonaActivator().ClearSinks();
        m_host->GetEditorProfileActivator().ClearSinks();
    }

    for (auto& panel : m_panels)
        panel->OnShutdown();

    m_panels.clear();
    m_productionDashboard = nullptr;
    m_window.reset();
}

// ─── Panel Management ─────────────────────────────────────────────────────────

void EditorShell::RegisterPanel(std::unique_ptr<IPanel> panel, UIDockArea area)
{
    panel->OnInit();
    const std::string panelId = panel->GetPanelId();
    m_window->DockPanel(panel->GetNativeWidget(),
                        panelId,
                        panel->GetTitle(),
                        ResolveCompositionDockArea(panelId, area));
    m_panels.push_back(std::move(panel));

    if (panelId == "saga.panel.production_dashboard")
    {
        m_host->GetCommandRegistry().Register({
            "saga.command.view.production_dashboard",
            "Production Dashboard",
            "View",
            [this, panelId]()
            {
                m_window->SetPanelVisible(panelId, true);
                m_window->FocusPanel(panelId);
                RefreshProductionDashboard();
            },
            true
        });
    }
    else if (panelId == "saga.panel.problems")
    {
        m_host->GetCommandRegistry().Register({
            "saga.command.view.diagnostics",
            "Diagnostics",
            "View",
            [this, panelId]()
            {
                m_window->SetPanelVisible(panelId, true);
                m_window->FocusPanel(panelId);
            },
            true
        });
    }
    else if (panelId == "saga.panel.customize_workspace")
    {
        m_host->GetCommandRegistry().Register({
            "saga.command.view.customize_workspace",
            "Customize Workspace",
            "View",
            [this, panelId]()
            {
                m_window->SetPanelVisible(panelId, true);
                m_window->FocusPanel(panelId);
            },
            true
        });
    }
    else if (panelId == "saga.panel.shortcut_preferences")
    {
        m_host->GetCommandRegistry().Register({
            "saga.command.edit.shortcut_preferences",
            "Shortcut Preferences",
            "Edit",
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
    else if (const EditorProfile* activeProfile =
                 m_host->GetEditorProfileRegistry().GetActive())
    {
        const bool shouldShow =
            std::find(activeProfile->defaultPanels.begin(),
                      activeProfile->defaultPanels.end(),
                      panelId) != activeProfile->defaultPanels.end();
        m_window->SetPanelVisible(panelId, shouldShow);
        if (shouldShow)
        {
            m_window->FocusPanel(panelId);
        }
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

// ─── Internals ────────────────────────────────────────────────────────────────

void EditorShell::RegisterDefaultPanels()
{
    auto dashboard = std::make_unique<ProductionDashboardPanel>(*m_host);
    m_productionDashboard = dashboard.get();
    RegisterPanel(std::move(dashboard), UIDockArea::Left);
    RegisterPanel(std::make_unique<HierarchyPanel>(),    UIDockArea::Left);
    RegisterPanel(std::make_unique<WorldViewportPanel>(), UIDockArea::Center);
    RegisterPanel(std::make_unique<InspectorPanel>(),    UIDockArea::Right);
    RegisterPanel(std::make_unique<AssetBrowserPanel>(), UIDockArea::Bottom);
    RegisterPanel(std::make_unique<ConsolePanel>(),      UIDockArea::Bottom);

    RegisterPanel(std::make_unique<CustomizeWorkspacePanel>(*m_host),
                  UIDockArea::Right);
    RegisterPanel(std::make_unique<ShortcutPreferencesPanel>(*m_host),
                  UIDockArea::Right);

    auto problems = std::make_unique<ProblemsPanel>();
    problems->SetDiagnosticsService(&m_host->GetEditorDiagnosticsService());
    RegisterPanel(std::move(problems), UIDockArea::Bottom);
}

void EditorShell::RegisterPersonaCommands()
{
    auto& registry = m_host->GetCommandRegistry();
    auto& personas = m_host->GetPersonaRegistry();

    const auto registerPersona = [&registry, &personas](std::string commandId,
                                                        std::string label,
                                                        std::string personaId)
    {
        registry.Register({
            std::move(commandId),
            std::move(label),
            "Persona",
            [&personas, personaId = std::move(personaId)]()
            {
                (void)personas.SetActive(personaId);
            },
            true
        });
    };

    registerPersona("saga.command.persona.beginner",
                    "Blocky Beginner",
                    "saga.persona.beginner");
    registerPersona("saga.command.persona.indie",
                    "Indie Balanced",
                    "saga.persona.indie");
    registerPersona("saga.command.persona.pro",
                    "Pro Dense",
                    "saga.persona.pro");
    registerPersona("saga.command.persona.technical",
                    "Technical",
                    "saga.persona.technical");
}

void EditorShell::RegisterProfileCommands()
{
    auto& registry = m_host->GetCommandRegistry();
    auto& profiles = m_host->GetEditorProfileRegistry();

    for (const EditorProfile& profile : profiles.GetAll())
    {
        registry.Register({
            ProfileCommandId(profile.id),
            profile.displayName,
            "Workspace Profile",
            [&profiles, profileId = profile.id]()
            {
                (void)profiles.SetActive(profileId);
            },
            true
        });
    }
}

void EditorShell::WirePersonaSinks()
{
    auto& activator = m_host->GetPersonaActivator();

    activator.SetThemeSink(
        [this](const std::string& themeId)
        {
            return m_host->GetThemeRegistry().Apply(themeId);
        });

    activator.SetPanelVisibilitySink(
        [this](const std::vector<std::string>& panelIds)
        {
            return ApplyPersonaPanelVisibility(panelIds);
        });

    m_personaSubscription = m_host->GetPersonaRegistry().OnPersonaChanged(
        [this](const UIPersona& persona)
        {
            const PersonaApplyResult result =
                m_host->GetPersonaActivator().Apply(persona);

            std::string message = "Persona: " + persona.displayName;
            if (!result.lastError.empty())
            {
                message += " (" + result.lastError + ")";
            }
            m_window->SetStatusMessage(message);
            RefreshProductionDashboard();
        });
}

void EditorShell::WireProfileSinks()
{
    auto& activator = m_host->GetEditorProfileActivator();

    activator.SetLayoutSink(
        [this](const std::string& layoutPresetId)
        {
            return ApplyProfileLayout(layoutPresetId);
        });

    activator.SetShortcutMapSink(
        [this](const EditorProfile& profile)
        {
            return ApplyProfileShortcutMap(profile);
        });

    activator.SetToolbarSink(
        [this](const std::vector<std::string>& commandIds)
        {
            return ApplyProfileToolbar(commandIds);
        });

    activator.SetPanelVisibilitySink(
        [this](const std::vector<std::string>& panelIds)
        {
            return ApplyProfilePanelVisibility(panelIds);
        });

    activator.SetShellChromeSink(
        [this](const EditorProfile& profile)
        {
            return ApplyProfileShellChrome(profile);
        });

    activator.SetToolVisibilitySink(
        [this](const std::vector<std::string>& commandIds)
        {
            return ApplyProfileToolVisibility(commandIds);
        });

    m_profileSubscription = m_host->GetEditorProfileRegistry().OnProfileChanged(
        [this](const EditorProfile& profile)
        {
            const EditorProfileApplyResult result =
                m_host->GetEditorProfileActivator().Apply(profile);

            std::string message = "Workspace Profile: " + profile.displayName;
            if (!result.lastError.empty())
            {
                message += " (" + result.lastError + ")";
            }
            m_window->SetStatusMessage(message);
        });
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

void EditorShell::ApplyActivePersona()
{
    const PersonaApplyResult result =
        m_host->GetPersonaActivator().ApplyActive(m_host->GetPersonaRegistry());

    const UIPersona* active = m_host->GetPersonaRegistry().GetActive();
    if (active != nullptr)
    {
        std::string message = "Persona: " + active->displayName;
        if (!result.lastError.empty())
        {
            message += " (" + result.lastError + ")";
        }
        m_window->SetStatusMessage(message);
        RefreshProductionDashboard();
    }
}

void EditorShell::ApplyActiveProfile()
{
    const EditorProfileApplyResult result =
        m_host->GetEditorProfileActivator().ApplyActive(
            m_host->GetEditorProfileRegistry());

    const EditorProfile* active = m_host->GetEditorProfileRegistry().GetActive();
    if (active != nullptr)
    {
        std::string message = "Workspace Profile: " + active->displayName;
        if (!result.lastError.empty())
        {
            message += " (" + result.lastError + ")";
        }
        m_window->SetStatusMessage(message);
        RefreshProductionDashboard();
    }
}

void EditorShell::RefreshProductionDashboard()
{
    if (m_productionDashboard != nullptr)
    {
        m_productionDashboard->Refresh();
    }
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

bool EditorShell::ApplyPersonaPanelVisibility(const std::vector<std::string>& panelIds)
{
    if (GetUsableCompositionSnapshot() != nullptr)
    {
        return ApplyCompositionPanelVisibility();
    }

    std::unordered_set<std::string> visible(panelIds.begin(), panelIds.end());

    for (const auto& panel : m_panels)
    {
        const std::string id = panel->GetPanelId();
        const bool shouldShow = visible.find(id) != visible.end();
        m_window->SetPanelVisible(id, shouldShow);
        if (shouldShow)
        {
            m_window->FocusPanel(id);
        }
    }

    return true;
}

bool EditorShell::ApplyProfileLayout(const std::string& layoutPresetId)
{
    m_activeLayoutPresetId = layoutPresetId;
    BuildDefaultLayout();
    (void)BuildCompositionLayout();
    m_window->ApplyShellLayout(m_layout);
    return true;
}

bool EditorShell::ApplyProfileShortcutMap(const EditorProfile& profile)
{
    if (GetUsableCompositionSnapshot() != nullptr)
    {
        return ApplyCompositionShortcuts();
    }

    auto& shortcuts = m_host->GetShortcutManager();
    shortcuts.Clear();
    for (const EditorShortcutBinding& binding : profile.shortcutBindings)
    {
        shortcuts.Bind(binding.chord, binding.commandId);
    }
    return true;
}

bool EditorShell::ApplyProfileToolbar(const std::vector<std::string>& commandIds)
{
    if (GetUsableCompositionSnapshot() != nullptr)
    {
        (void)BuildCompositionLayout();
        m_window->ApplyShellLayout(m_layout);
        return true;
    }

    m_activeToolbarCommands = commandIds;
    m_layout.mainToolbarItems.clear();

    for (const std::string& commandId : commandIds)
    {
        const CommandDescriptor* command =
            m_host->GetCommandRegistry().Find(commandId);
        const std::string label = command ? command->label : commandId;
        m_layout.mainToolbarItems.push_back({ label, commandId, "", false });
    }

    m_window->ApplyShellLayout(m_layout);
    return true;
}

bool EditorShell::ApplyProfilePanelVisibility(const std::vector<std::string>& panelIds)
{
    if (GetUsableCompositionSnapshot() != nullptr)
    {
        return ApplyCompositionPanelVisibility();
    }

    std::unordered_set<std::string> visible(panelIds.begin(), panelIds.end());

    for (const auto& panel : m_panels)
    {
        const std::string id = panel->GetPanelId();
        const bool shouldShow = visible.find(id) != visible.end();
        m_window->SetPanelVisible(id, shouldShow);
        if (shouldShow)
        {
            m_window->FocusPanel(id);
        }
    }

    return true;
}

bool EditorShell::ApplyProfileShellChrome(const EditorProfile& profile)
{
    m_layout.showMenuBar = profile.showMenuBar;
    m_layout.showStatusBar = profile.showStatusBar;
    m_layout.showMainToolbar = profile.showMainToolbar;
    m_window->SetTitle(m_baseWindowTitle + ProfileTitleSuffix(profile));
    m_window->ApplyShellLayout(m_layout);
    return true;
}

bool EditorShell::ApplyProfileToolVisibility(const std::vector<std::string>& commandIds)
{
    m_activeToolCommands = commandIds;

    auto& registry = m_host->GetCommandRegistry();
    const std::unordered_set<std::string> visible(commandIds.begin(), commandIds.end());
    for (const std::string& commandId : ProfileToolCommandUniverse())
    {
        registry.SetEnabled(commandId, visible.find(commandId) != visible.end());
    }

    return true;
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
        { "New Scene",       "saga.command.file.new_scene",     "Ctrl+N",       false },
        { "Open Project…",   "saga.command.file.open_project",  "Ctrl+O",       false },
        { "Save Scene",      "saga.command.file.save",          "Ctrl+S",       false },
        { "",                "",                                "",             true  },
        { "Exit",            "saga.command.file.exit",          "",             false },
    };

    // ── Edit menu ─────────────────────────────────────────────────────────────
    MenuDescriptor edit;
    edit.title = "Edit";
    edit.items = {
        { "Undo",            "saga.command.edit.undo",          "Ctrl+Z",       false },
        { "Redo",            "saga.command.edit.redo",          "Ctrl+Shift+Z", false },
        { "",                "",                                "",             true  },
        { "Preferences…",    "saga.command.edit.preferences",   "",             false },
        { "Shortcut Preferences", "saga.command.edit.shortcut_preferences", "",  false },
    };

    // ── View menu ─────────────────────────────────────────────────────────────
    MenuDescriptor view;
    view.title = "View";
    view.items = {
        { "Hierarchy",       "saga.command.view.hierarchy",     "",             false },
        { "Inspector",       "saga.command.view.inspector",     "",             false },
        { "Console",         "saga.command.view.console",       "",             false },
        { "Asset Browser",   "saga.command.view.asset_browser", "",             false },
        { "Production",      "saga.command.view.production_dashboard", "",      false },
        { "Customize Workspace", "saga.command.view.customize_workspace", "",   false },
        { "",                "",                                "",             true  },
        { "Theme…",          "saga.command.view.theme",         "",             false },
        { "",                "",                                "",             true  },
        { "Basic",           "saga.command.profile.basic",              "",     false },
        { "Node Editor",     "saga.command.profile.node_editor",        "",     false },
        { "Standard Pipeline","saga.command.profile.standard_pipeline", "",     false },
        { "Advanced Pipeline","saga.command.profile.advanced_pipeline", "",     false },
        { "Custom",          "saga.command.profile.custom",             "",     false },
        { "",                "",                                "",             true  },
        { "Blocky Beginner", "saga.command.persona.beginner",   "",             false },
        { "Indie Balanced",  "saga.command.persona.indie",      "",             false },
        { "Pro Dense",       "saga.command.persona.pro",        "",             false },
        { "Technical",       "saga.command.persona.technical",  "",             false },
    };

    // ── World menu ────────────────────────────────────────────────────────────
    MenuDescriptor world;
    world.title = "World";
    world.items = {
        { "Add Entity",      "saga.command.world.add_entity",   "",             false },
        { "Bake Lighting",   "saga.command.world.bake_lighting","",             false },
        { "World Settings…", "saga.command.world.settings",     "",             false },
    };

    m_layout.menus = { file, edit, view, world };
}

} // namespace SagaEditor
