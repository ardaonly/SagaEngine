/// @file ComponentEditorRegistry.cpp
/// @brief Implementation of the component-typed inspector editor registry.

#include "SagaEditor/InspectorEditing/ComponentEditorRegistry.h"

#include "SagaEditor/Panels/IPanel.h"

#include <utility>

namespace SagaEditor
{

// ─── Registration ─────────────────────────────────────────────────────────────

void ComponentEditorRegistry::Register(std::type_index componentType,
                                       std::string     displayName,
                                       FactoryFn       factory)
{
    Entry entry{ ComponentEditorDescriptor{ componentType,
                                            std::move(displayName) },
                 std::move(factory) };

    // unordered_map::insert_or_assign keeps the registry idempotent:
    // re-registering the same type overwrites the previous entry
    // instead of stacking duplicate factories.
    m_entries.insert_or_assign(componentType, std::move(entry));
}

bool ComponentEditorRegistry::Unregister(std::type_index componentType)
{
    return m_entries.erase(componentType) != 0;
}

void ComponentEditorRegistry::Clear() noexcept
{
    m_entries.clear();
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

bool ComponentEditorRegistry::Has(std::type_index componentType) const noexcept
{
    return m_entries.find(componentType) != m_entries.end();
}

std::unique_ptr<IPanel>
ComponentEditorRegistry::Create(std::type_index componentType) const
{
    const auto it = m_entries.find(componentType);
    if (it == m_entries.end() || !it->second.factory)
    {
        return nullptr;
    }

    // The factory itself is allowed to return nullptr to signal a
    // soft "no editor available right now" — we forward that decision.
    return it->second.factory();
}

const ComponentEditorDescriptor*
ComponentEditorRegistry::Find(std::type_index componentType) const noexcept
{
    const auto it = m_entries.find(componentType);
    return it == m_entries.end() ? nullptr : &it->second.descriptor;
}

std::size_t ComponentEditorRegistry::Size() const noexcept
{
    return m_entries.size();
}

std::vector<ComponentEditorDescriptor>
ComponentEditorRegistry::Descriptors() const
{
    std::vector<ComponentEditorDescriptor> out;
    out.reserve(m_entries.size());
    for (const auto& [_, entry] : m_entries)
    {
        out.push_back(entry.descriptor);
    }
    return out;
}

} // namespace SagaEditor
