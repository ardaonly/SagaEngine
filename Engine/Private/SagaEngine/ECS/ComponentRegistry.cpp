/// @file ComponentRegistry.cpp
/// @brief Non-template method implementations for ComponentRegistry.

#include "SagaEngine/ECS/ComponentRegistry.h"

namespace SagaEngine::ECS {

// ─── Singleton ────────────────────────────────────────────────────────────────

ComponentRegistry& ComponentRegistry::Instance() noexcept
{
    static ComponentRegistry s_instance;
    return s_instance;
}

// ─── Lookup ───────────────────────────────────────────────────────────────────

ComponentTypeId ComponentRegistry::GetId(std::type_index type) const
{
    std::lock_guard lock(m_mutex);

    const auto it = m_typeToId.find(type);
    if (it == m_typeToId.end())
    {
        LOG_ERROR("ComponentRegistry",
            "Unregistered type_index: %s.", type.name());
        throw std::logic_error("ComponentRegistry: type not registered");
    }
    return it->second;
}

std::string_view ComponentRegistry::GetName(ComponentTypeId id) const noexcept
{
    std::lock_guard lock(m_mutex);

    const auto it = m_idToName.find(id);
    if (it == m_idToName.end())
        return "<unknown>";

    return std::string_view(it->second);
}

// ─── Predicates ───────────────────────────────────────────────────────────────

bool ComponentRegistry::IsIdTaken(ComponentTypeId id) const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_idToType.contains(id);
}

// ─── Maintenance ──────────────────────────────────────────────────────────────

void ComponentRegistry::Reset() noexcept
{
    std::lock_guard lock(m_mutex);
    m_typeToId.clear();
    m_idToType.clear();
    m_idToName.clear();
}

} // namespace SagaEngine::ECS
