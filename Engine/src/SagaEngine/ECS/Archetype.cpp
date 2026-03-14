#include "SagaEngine/ECS/Archetype.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>
#include <functional>

namespace SagaEngine::ECS
{

Archetype::Archetype() = default;

Archetype::Archetype(std::vector<ComponentTypeId> componentTypes)
    : m_ComponentTypes(std::move(componentTypes))
{
    std::sort(m_ComponentTypes.begin(), m_ComponentTypes.end());
}

bool Archetype::HasComponent(ComponentTypeId type) const
{
    return std::find(m_ComponentTypes.begin(), m_ComponentTypes.end(), type) != m_ComponentTypes.end();
}

void Archetype::AddComponentType(ComponentTypeId type)
{
    if (!HasComponent(type)) {
        m_ComponentTypes.push_back(type);
        std::sort(m_ComponentTypes.begin(), m_ComponentTypes.end());
    }
}

void Archetype::AddEntity(EntityId entity)
{
    m_Entities.push_back(entity);
}

void Archetype::RemoveEntity(EntityId entity)
{
    auto it = std::find(m_Entities.begin(), m_Entities.end(), entity);
    if (it != m_Entities.end()) {
        *it = m_Entities.back();
        m_Entities.pop_back();
    }
}

bool Archetype::operator==(const Archetype& other) const
{
    return m_ComponentTypes == other.m_ComponentTypes;
}

size_t Archetype::GetHash() const
{
    size_t hash = 0;
    std::hash<ComponentTypeId> hasher;
    for (auto type : m_ComponentTypes) {
        hash ^= hasher(type) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

// ArchetypeManager

Archetype& ArchetypeManager::GetOrCreateArchetype(const std::vector<ComponentTypeId>& types)
{
    Archetype query(types);
    size_t hash = query.GetHash();
    
    for (auto& archetype : m_Archetypes) {
        if (archetype->GetHash() == hash && *archetype == query) {
            return *archetype;
        }
    }
    
    m_Archetypes.push_back(std::make_unique<Archetype>(types));
    return *m_Archetypes.back();
}

Archetype* ArchetypeManager::GetArchetype(const std::vector<ComponentTypeId>& types)
{
    Archetype query(types);
    size_t hash = query.GetHash();
    
    for (auto& archetype : m_Archetypes) {
        if (archetype->GetHash() == hash && *archetype == query) {
            return archetype.get();
        }
    }
    
    return nullptr;
}

} // namespace SagaEngine::ECS