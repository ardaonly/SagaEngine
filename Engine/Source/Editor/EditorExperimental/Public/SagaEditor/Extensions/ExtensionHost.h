// SPDX-License-Identifier: MPL-2.0
/// @file ExtensionHost.h
/// @brief Drives extension lifecycle — load, initialise, and unload.

#pragma once

#include "SagaEditor/Extensions/IExtensionContext.h"
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor
{

class ExtensionRegistry;
class EditorHost;
class EditorShell;

// ─── Extension Host ───────────────────────────────────────────────────────────

/// Owns the IExtensionContext implementation and drives OnLoad / OnUnload for
/// every registered extension. Panel registration is available after the editor
/// shell binds itself during initialisation.
class ExtensionHost
{
public:
    ExtensionHost(ExtensionRegistry& registry, EditorHost& host);
    ~ExtensionHost();

    /// Bind the shell pointer so extensions can register panels.
    /// EditorShell::Init performs this binding; exposed for explicit hosts.
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
    std::vector<std::string> m_loadedExtensionIds;

    /// Context implementation forwarded to extensions; declared in .cpp.
    struct ContextImpl;
    std::unique_ptr<ContextImpl> m_context;
};

} // namespace SagaEditor
