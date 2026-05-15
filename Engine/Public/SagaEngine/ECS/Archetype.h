#pragma once
#include "Entity.h"
#include "Component.h"
#include <vector>
#include <unordered_set>
#include <memory>

namespace SagaEngine::ECS
{

class Archetype
{
public:
    Archetype();
    explicit Archetype(std::vector<ComponentTypeId> componentTypes);
    
    const std::vector<ComponentTypeId>& GetComponentTypes() const { return m_ComponentTypes; }
    bool HasComponent(ComponentTypeId type) const;
    void AddComponentType(ComponentTypeId type);
    
    size_t GetEntityCount() const { return m_Entities.size(); }
    const std::vector<EntityId>& GetEntities() const { return m_Entities; }
    
    void AddEntity(EntityId entity);
    void RemoveEntity(EntityId entity);
    
    bool operator==(const Archetype& other) const;
    size_t GetHash() const;
    
private:
    std::vector<ComponentTypeId> m_ComponentTypes;
    std::vector<EntityId> m_Entities;
};

class ArchetypeManager
{
public:
    Archetype& GetOrCreateArchetype(const std::vector<ComponentTypeId>& types);
    Archetype* GetArchetype(const std::vector<ComponentTypeId>& types);
    
    const std::vector<std::unique_ptr<Archetype>>& GetAllArchetypes() const { return m_Archetypes; }
    
private:
    std::vector<std::unique_ptr<Archetype>> m_Archetypes;
};

} // namespace SagaEngine::ECS