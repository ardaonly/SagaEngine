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

    RegisterShellCommands(host);
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
    // ── File menu ─────────────────────────────────────────────────────────────
    MenuDescriptor file;
    file.label = "File";
    file.items = {
        { "New Scene",      false, false, nullptr },
        { "Open Project…",  false, false, nullptr },
        { "Save Scene",     false, false, nullptr },
        { "",               false, true,  nullptr },
        { "Exit",           false, false, nullptr },
    };

    // ── Edit menu ─────────────────────────────────────────────────────────────
    MenuDescriptor edit;
    edit.label = "Edit";
    edit.items = {
        { "Undo",  false, false, nullptr },
        { "Redo",  false, false, nullptr },
        { "",      false, true,  nullptr },
        { "Preferences…", false, false, nullptr },
    };

    // ── View menu ─────────────────────────────────────────────────────────────
    MenuDescriptor view;
    view.label = "View";
    view.items = {
        { "Hierarchy",     false, false, nullptr },
        { "Inspector",     false, false, nullptr },
        { "Console",       false, false, nullptr },
        { "Asset Browser", false, false, nullptr },
        { "",              false, true,  nullptr },
        { "Theme…",        false, false, nullptr },
    };

    // ── World menu ────────────────────────────────────────────────────────────
    MenuDescriptor world;
    world.label = "World";
    world.items = {
        { "Add Entity",    false, false, nullptr },
        { "Bake Lighting", false, false, nullptr },
        { "World Settings…", false, false, nullptr },
    };

    m_layout.menus = { file, edit, view, world };
}

} // namespace SagaEditor
