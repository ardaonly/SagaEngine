/// @file ExtensionHost.cpp
/// @brief Drives extension lifecycle — load, initialise, and unload.

#include "SagaEditor/Extensions/ExtensionHost.h"
#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/IEditorExtension.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Shell/EditorShell.h"

#include <vector>
#include <algorithm>

namespace SagaEditor
{

// ─── Context Implementation ───────────────────────────────────────────────────

/// Concrete IExtensionContext forwarded to every extension on load/unload.
struct ExtensionHost::ContextImpl : public IExtensionContext
{
    EditorHost&   host;
    EditorShell*  shell = nullptr;
    std::vector<std::string> registeredCommands; ///< Tracked for automatic unregister.

    explicit ContextImpl(EditorHost& h) : host(h) {}

    // ─── Panel Registration ───────────────────────────────────────────────────

    void RegisterPanel(std::unique_ptr<IPanel> panel,
                       Qt::DockWidgetArea area) override
    {
        if (shell)
            shell->RegisterPanel(std::move(panel), area);
    }

    void UnregisterPanel(const std::string& /*panelId*/) override
    {
        // Deregistration path wired when panel removal is added to EditorShell.
    }

    // ─── Command Registration ─────────────────────────────────────────────────

    void RegisterCommand(CommandDescriptor descriptor) override
    {
        registeredCommands.push_back(descriptor.id);
        host.GetCommandRegistry().Register(std::move(descriptor));
    }

    void UnregisterCommand(const std::string& commandId) override
    {
        host.GetCommandRegistry().Unregister(commandId);
        registeredCommands.erase(
            std::remove(registeredCommands.begin(), registeredCommands.end(), commandId),
            registeredCommands.end());
    }

    // ─── Theme Registration ───────────────────────────────────────────────────

    void RegisterTheme(EditorTheme theme) override
    {
        host.GetThemeRegistry().Register(std::move(theme));
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
    m_shell                = &shell;
    m_context->shell       = &shell;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void ExtensionHost::LoadAll()
{
    for (auto* ext : m_registry.GetAll())
        ext->OnLoad(*m_context);
}

void ExtensionHost::Load(const std::string& extensionId)
{
    if (auto* ext = m_registry.Find(extensionId))
        ext->OnLoad(*m_context);
}

void ExtensionHost::Unload(const std::string& extensionId)
{
    if (auto* ext = m_registry.Find(extensionId))
        ext->OnUnload(*m_context);
}

void ExtensionHost::ShutdownAll()
{
    auto all = m_registry.GetAll();
    for (auto it = all.rbegin(); it != all.rend(); ++it)
        (*it)->OnUnload(*m_context);
}

} // namespace SagaEditor
