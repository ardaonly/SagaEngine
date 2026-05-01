/// @file EditorShell.cpp
/// @brief Main editor shell implementation.

#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/UI/IUIFactory.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditor/Shell/ShellCommands.h"
#include "SagaEditor/Panels/IPanel.h"

// Default Qt panels (concrete types live in the Qt backend)
#include "SagaEditor/Panels/HierarchyPanel.h"
#include "SagaEditor/Panels/InspectorPanel.h"
#include "SagaEditor/Panels/ConsolePanel.h"
#include "SagaEditor/Panels/AssetBrowserPanel.h"
#include "SagaEditor/Panels/WorldViewportPanel.h"

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

EditorShell::EditorShell()  = default;
EditorShell::~EditorShell() = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool EditorShell::Init(EditorHost&              host,
                       IUIFactory&              factory,
                       const EditorShellConfig& cfg)
{
    m_host = &host;

    m_window = factory.CreateMainWindow(cfg.windowTitle, cfg.windowWidth, cfg.windowHeight);
    if (!m_window)
        return false;

    // Wire close → quit the event loop.
    m_window->SetOnClose([]{});

    BuildDefaultLayout();
    m_window->ApplyShellLayout(m_layout);

    // RegisterShellCommands takes (registry, undoStack); the older one-arg
    // overload that took the whole host doesn't exist. Pull both services
    // off the host directly.
    RegisterShellCommands(host.GetCommandRegistry(),
                          host.GetUndoRedoStack());
    RegisterDefaultPanels();

    if (cfg.maximized)
        m_window->ShowMaximized();
    else
        m_window->Show();

    return true;
}

void EditorShell::Shutdown()
{
    for (auto& panel : m_panels)
        panel->OnShutdown();

    m_panels.clear();
    m_window.reset();
}

// ─── Panel Management ─────────────────────────────────────────────────────────

void EditorShell::RegisterPanel(std::unique_ptr<IPanel> panel, UIDockArea area)
{
    panel->OnInit();
    m_window->DockPanel(panel->GetNativeWidget(), panel->GetPanelId(), panel->GetTitle(), area);
    m_panels.push_back(std::move(panel));
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

// ─── Accessors ────────────────────────────────────────────────────────────────

IUIMainWindow& EditorShell::GetMainWindow() noexcept { return *m_window; }
EditorHost&    EditorShell::GetHost()       noexcept { return *m_host; }

// ─── Internals ────────────────────────────────────────────────────────────────

void EditorShell::RegisterDefaultPanels()
{
    RegisterPanel(std::make_unique<HierarchyPanel>(),    UIDockArea::Left);
    RegisterPanel(std::make_unique<WorldViewportPanel>(), UIDockArea::Center);
    RegisterPanel(std::make_unique<InspectorPanel>(),    UIDockArea::Right);
    RegisterPanel(std::make_unique<AssetBrowserPanel>(), UIDockArea::Bottom);
    RegisterPanel(std::make_unique<ConsolePanel>(),      UIDockArea::Bottom);
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
    };

    // ── View menu ─────────────────────────────────────────────────────────────
    MenuDescriptor view;
    view.title = "View";
    view.items = {
        { "Hierarchy",       "saga.command.view.hierarchy",     "",             false },
        { "Inspector",       "saga.command.view.inspector",     "",             false },
        { "Console",         "saga.command.view.console",       "",             false },
        { "Asset Browser",   "saga.command.view.asset_browser", "",             false },
        { "",                "",                                "",             true  },
        { "Theme…",          "saga.command.view.theme",         "",             false },
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
