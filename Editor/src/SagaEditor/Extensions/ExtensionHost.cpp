/// @file ExtensionHost.cpp
/// @brief Drives extension lifecycle — load, initialise, and unload.

#include "SagaEditor/Extensions/ExtensionHost.h"

#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/IEditorExtension.h"
#include "SagaEditor/Extensions/IExtensionContext.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/Themes/EditorTheme.h"
#include "SagaEditor/Themes/ThemeRegistry.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <algorithm>
#include <vector>

namespace SagaEditor
{

namespace
{

// ─── DockArea → UIDockArea Translation ────────────────────────────────────────

/// `IExtensionContext::DockArea` is the framework-free enum extensions
/// see; `UIDockArea` is the value `EditorShell::RegisterPanel` consumes.
/// Centralising the mapping here keeps every other layer decoupled from
/// the extension-context shape.
[[nodiscard]] UIDockArea TranslateDockArea(DockArea area) noexcept
{
    switch (area)
    {
        case DockArea::Left:   return UIDockArea::Left;
        case DockArea::Right:  return UIDockArea::Right;
        case DockArea::Top:    return UIDockArea::Top;
        case DockArea::Bottom: return UIDockArea::Bottom;
    }
    return UIDockArea::Right;
}

} // namespace

// ─── Context Implementation ───────────────────────────────────────────────────

/// Concrete IExtensionContext forwarded to every extension on load/unload.
struct ExtensionHost::ContextImpl : public IExtensionContext
{
    EditorHost&              host;
    EditorShell*             shell = nullptr;
    std::vector<std::string> registeredCommands; ///< Tracked for automatic unregister.

    explicit ContextImpl(EditorHost& h) : host(h) {}

    // ─── Panel Registration ───────────────────────────────────────────────────

    void RegisterPanel(std::unique_ptr<IPanel> panel,
                       DockArea area = DockArea::Right) override
    {
        if (shell)
        {
            shell->RegisterPanel(std::move(panel), TranslateDockArea(area));
        }
    }

    void UnregisterPanel(const std::string& /*panelId*/) override
    {
        // Deregistration path wired when panel removal is added to EditorShell.
    }

    // ─── Command Registration ─────────────────────────────────────────────────

    void RegisterCommand(const CommandDescriptor& descriptor) override
    {
        registeredCommands.push_back(descriptor.id);
        host.GetCommandRegistry().Register(descriptor);
    }

    void UnregisterCommand(const std::string& commandId) override
    {
        host.GetCommandRegistry().Unregister(commandId);
        registeredCommands.erase(
            std::remove(registeredCommands.begin(),
                        registeredCommands.end(),
                        commandId),
            registeredCommands.end());
    }

    // ─── Theme Registration ───────────────────────────────────────────────────

    void RegisterTheme(const EditorTheme& theme) override
    {
        host.GetThemeRegistry().Register(theme);
    }

    // ─── Host Access ──────────────────────────────────────────────────────────

    EditorHost& GetHost() override { return host; }
};

// ─── Construction ─────────────────────────────────────────────────────────────

ExtensionHost::ExtensionHost(ExtensionRegistry& registry, EditorHost& host)
    : m_registry(registry)
    , m_host(host)
    , m_context(std::make_unique<ContextImpl>(host))
{}

ExtensionHost::~ExtensionHost() = default;

void ExtensionHost::SetShell(EditorShell& shell)
{
    m_shell          = &shell;
    m_context->shell = &shell;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void ExtensionHost::LoadAll()
{
    for (auto* ext : m_registry.GetAll())
    {
        ext->OnLoad(*m_context);
    }
}

void ExtensionHost::Load(const std::string& extensionId)
{
    if (auto* ext = m_registry.Find(extensionId))
    {
        ext->OnLoad(*m_context);
    }
}

void ExtensionHost::Unload(const std::string& extensionId)
{
    if (auto* ext = m_registry.Find(extensionId))
    {
        ext->OnUnload(*m_context);
    }
}

void ExtensionHost::ShutdownAll()
{
    auto all = m_registry.GetAll();
    for (auto it = all.rbegin(); it != all.rend(); ++it)
    {
        (*it)->OnUnload(*m_context);
    }
}

} // namespace SagaEditor
