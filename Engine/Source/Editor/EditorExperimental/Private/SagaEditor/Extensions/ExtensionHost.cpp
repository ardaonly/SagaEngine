// SPDX-License-Identifier: MPL-2.0
/// @file ExtensionHost.cpp
/// @brief Drives extension lifecycle — load, initialise, and unload.

#include "SagaEditor/Extensions/ExtensionHost.h"

#include "SagaEditor/Commands/CommandRegistry.h"
#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
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
#include <optional>
#include <unordered_map>
#include <utility>
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
    struct OwnedRegistrations
    {
        std::vector<std::string> panels;
        std::vector<std::string> commands;
    };

    EditorHost&              host;
    EditorShell*             shell = nullptr;
    std::optional<std::string> activeExtensionId;
    std::unordered_map<std::string, OwnedRegistrations> registrations;

    explicit ContextImpl(EditorHost& h) : host(h) {}

    void BeginLifecycle(const std::string& extensionId)
    {
        activeExtensionId = extensionId;
    }

    void EndLifecycle() noexcept
    {
        activeExtensionId.reset();
    }

    void ReportRejectedMutation(const std::string& code,
                                const std::string& message)
    {
        const std::string source = "editor.extension.host";
        EditorDiagnostic diagnostic;
        diagnostic.severity = EditorDiagnosticSeverity::Warning;
        diagnostic.source = source;
        diagnostic.code = code;
        diagnostic.message = message;
        host.GetEditorDiagnosticsService().ReplaceSource(
            source, {std::move(diagnostic)});
    }

    [[nodiscard]] bool HasActiveExtension(const char* operation)
    {
        if (activeExtensionId)
            return true;
        ReportRejectedMutation(
            "Editor.Extension.RegistrationOutsideLifecycle",
            std::string("Rejected extension context operation outside "
                        "OnLoad/OnUnload: ") + operation);
        return false;
    }

    [[nodiscard]] OwnedRegistrations& ActiveRegistrations()
    {
        return registrations[*activeExtensionId];
    }

    static bool EraseOwned(std::vector<std::string>& ids,
                           const std::string& id)
    {
        const auto it = std::find(ids.begin(), ids.end(), id);
        if (it == ids.end())
            return false;
        ids.erase(it);
        return true;
    }

    // ─── Panel Registration ───────────────────────────────────────────────────

    void RegisterPanel(std::unique_ptr<IPanel> panel,
                       DockArea area = DockArea::Right) override
    {
        if (!HasActiveExtension("RegisterPanel"))
            return;
        if (!panel)
        {
            ReportRejectedMutation(
                "Editor.Extension.InvalidPanel",
                "Rejected extension registration of a null panel.");
            return;
        }
        if (!shell)
        {
            ReportRejectedMutation(
                "Editor.Extension.ShellUnavailable",
                "Rejected extension panel registration because the editor "
                "shell is unavailable.");
            return;
        }

        auto& owned = ActiveRegistrations().panels;
        owned.push_back(panel->GetPanelId());
        shell->RegisterPanel(std::move(panel), TranslateDockArea(area));
    }

    void UnregisterPanel(const std::string& panelId) override
    {
        if (!HasActiveExtension("UnregisterPanel"))
            return;
        auto& owned = ActiveRegistrations().panels;
        if (!EraseOwned(owned, panelId))
            return;
        if (shell)
            (void)shell->UnregisterPanel(panelId);
    }

    // ─── Command Registration ─────────────────────────────────────────────────

    void RegisterCommand(const CommandDescriptor& descriptor) override
    {
        if (!HasActiveExtension("RegisterCommand"))
            return;
        ActiveRegistrations().commands.push_back(descriptor.id);
        host.GetCommandRegistry().Register(descriptor);
    }

    void UnregisterCommand(const std::string& commandId) override
    {
        if (!HasActiveExtension("UnregisterCommand"))
            return;
        auto& owned = ActiveRegistrations().commands;
        if (!EraseOwned(owned, commandId))
            return;
        host.GetCommandRegistry().Unregister(commandId);
    }

    // ─── Theme Registration ───────────────────────────────────────────────────

    void RegisterTheme(const EditorTheme& theme) override
    {
        if (!HasActiveExtension("RegisterTheme"))
            return;
        host.GetThemeRegistry().Register(theme);
    }

    // ─── Host Access ──────────────────────────────────────────────────────────

    EditorHost& GetHost() override { return host; }

    void Cleanup(const std::string& extensionId)
    {
        const auto found = registrations.find(extensionId);
        if (found == registrations.end())
            return;

        auto& owned = found->second;
        for (auto it = owned.commands.rbegin(); it != owned.commands.rend(); ++it)
            host.GetCommandRegistry().Unregister(*it);
        if (shell)
            for (auto it = owned.panels.rbegin(); it != owned.panels.rend(); ++it)
                (void)shell->UnregisterPanel(*it);
        registrations.erase(found);
    }
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
    m_context->shell = &shell;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void ExtensionHost::LoadAll()
{
    for (auto* ext : m_registry.GetAll())
    {
        Load(ext->GetExtensionId());
    }
}

void ExtensionHost::Load(const std::string& extensionId)
{
    if (std::find(m_loadedExtensionIds.begin(),
                  m_loadedExtensionIds.end(),
                  extensionId) != m_loadedExtensionIds.end())
        return;

    if (auto* ext = m_registry.Find(extensionId))
    {
        m_context->BeginLifecycle(extensionId);
        try
        {
            ext->OnLoad(*m_context);
            m_context->EndLifecycle();
            m_loadedExtensionIds.push_back(extensionId);
        }
        catch (...)
        {
            m_context->EndLifecycle();
            m_context->Cleanup(extensionId);
            throw;
        }
    }
}

void ExtensionHost::Unload(const std::string& extensionId)
{
    const auto loaded = std::find(
        m_loadedExtensionIds.begin(),
        m_loadedExtensionIds.end(),
        extensionId);
    if (loaded == m_loadedExtensionIds.end())
        return;

    try
    {
        if (auto* ext = m_registry.Find(extensionId))
        {
            m_context->BeginLifecycle(extensionId);
            ext->OnUnload(*m_context);
            m_context->EndLifecycle();
        }
    }
    catch (...)
    {
        m_context->EndLifecycle();
        m_context->Cleanup(extensionId);
        m_loadedExtensionIds.erase(loaded);
        throw;
    }
    m_context->Cleanup(extensionId);
    m_loadedExtensionIds.erase(loaded);
}

void ExtensionHost::ShutdownAll()
{
    while (!m_loadedExtensionIds.empty())
    {
        Unload(m_loadedExtensionIds.back());
    }
}

} // namespace SagaEditor
