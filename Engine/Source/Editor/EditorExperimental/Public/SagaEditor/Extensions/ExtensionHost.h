/// @file ExtensionHost.h
/// @brief Drives extension lifecycle — load, initialise, and unload.

#pragma once

#include "SagaEditor/Extensions/IExtensionContext.h"
#include <string>

namespace SagaEditor
{

class ExtensionRegistry;
class EditorHost;
class EditorShell;

// ─── Extension Host ───────────────────────────────────────────────────────────

/// Owns the IExtensionContext implementation and drives OnLoad / OnUnload for
/// every registered extension. Extensions loaded before the shell is visible
/// may register panels; those loaded after the shell is open go live immediately.
class ExtensionHost
{
public:
    ExtensionHost(ExtensionRegistry& registry, EditorHost& host);
    ~ExtensionHost();

    /// Bind the shell pointer so extensions can register panels.
    /// Must be called after EditorShell::Init and before LoadAll.
    void SetShell(EditorShell& shell);

    /// Call OnLoad on every registered extension in registration order.
    void LoadAll();

    /// Call OnLoad for a single extension by id.
    void Load(const std::string& extensionId);

    /// Call OnUnload for a single extension by id.
    void Unload(const std::string& extensionId);

    /// Call OnUnload on all loaded extensions in reverse-load order.
    void ShutdownAll();

private:
    ExtensionRegistry& m_registry;
    EditorHost&        m_host;
    EditorShell*       m_shell = nullptr; ///< Non-owning; set via SetShell.

    /// Context implementation forwarded to extensions; declared in .cpp.
    struct ContextImpl;
    std::unique_ptr<ContextImpl> m_context;
};

} // namespace SagaEditor
