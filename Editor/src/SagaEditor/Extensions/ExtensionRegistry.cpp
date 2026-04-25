/// @file ExtensionRegistry.cpp
/// @brief Catalogue of all installed editor extensions.

#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/IEditorExtension.h"
#include <algorithm>

namespace SagaEditor
{

// ─── Registration ─────────────────────────────────────────────────────────────

void ExtensionRegistry::Register(std::unique_ptr<IEditorExtension> extension)
{
    const std::string id = extension->GetExtensionId().toStdString();

    if (!m_extensions.count(id))
        m_order.push_back(id);

    m_extensions[id] = std::move(extension);
}

void ExtensionRegistry::Unregister(const std::string& extensionId)
{
    m_extensions.erase(extensionId);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), extensionId), m_order.end());
}

// ─── Queries ──────────────────────────────────────────────────────────────────

IEditorExtension* ExtensionRegistry::Find(const std::string& extensionId) const
{
    auto it = m_extensions.find(extensionId);
    return (it != m_extensions.end()) ? it->second.get() : nullptr;
}

std::vector<IEditorExtension*> ExtensionRegistry::GetAll() const
{
    std::vector<IEditorExtension*> result;
    result.reserve(m_order.size());
    for (const auto& id : m_order)
    {
        auto it = m_extensions.find(id);
        if (it != m_extensions.end())
            result.push_back(it->second.get());
    }
    return result;
}

} // namespace SagaEditor
