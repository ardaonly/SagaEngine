/// @file ExtensionRegistry.h
/// @brief Catalogue of all installed editor extensions.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

class IEditorExtension;

// ─── Extension Registry ───────────────────────────────────────────────────────

/// Stores extension instances indexed by their stable extension id.
/// ExtensionHost reads from here when driving load/unload lifecycle.
class ExtensionRegistry
{
public:
    ExtensionRegistry();
    ~ExtensionRegistry(); ///< Out-of-line so the `unique_ptr<IEditorExtension>`
                          ///< deleter only instantiates inside the .cpp where
                          ///< `IEditorExtension` is the full type.

    ExtensionRegistry(const ExtensionRegistry&)            = delete;
    ExtensionRegistry& operator=(const ExtensionRegistry&) = delete;
    ExtensionRegistry(ExtensionRegistry&&) noexcept;
    ExtensionRegistry& operator=(ExtensionRegistry&&) noexcept;

    /// Register an extension. Replaces any existing registration with the same id.
    void Register(std::unique_ptr<IEditorExtension> extension);

    /// Remove an extension by id (must be unloaded before removal).
    void Unregister(const std::string& extensionId);

    /// Look up an extension by id. Returns nullptr if not found.
    [[nodiscard]] IEditorExtension* Find(const std::string& extensionId) const;

    /// Return all registered extensions in insertion order.
    [[nodiscard]] std::vector<IEditorExtension*> GetAll() const;

private:
    std::unordered_map<std::string, std::unique_ptr<IEditorExtension>> m_extensions;
    std::vector<std::string> m_order; ///< Preserves registration order for load sequencing.
};

} // namespace SagaEditor
