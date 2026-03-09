#pragma once
#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/Archetype.h"
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <mutex>
#include <functional>

namespace SagaEngine::Simulation
{

using EntityCallback = std::function<void(ECS::EntityId)>;
using ComponentCallback = std::function<void(ECS::EntityId, ECS::ComponentTypeId)>;

class WorldState
{
public:
    WorldState();
    ~WorldState();
    
    // Entity Management
    ECS::Entity CreateEntity();
    void DestroyEntity(ECS::EntityId entityId);
    bool IsEntityAlive(ECS::EntityId entityId) const;
    std::vector<ECS::EntityId> GetAllEntities() const;
    
    // Component Management
    template<typename T>
    void AddComponent(ECS::EntityId entityId, const T& component);
    
    template<typename T>
    T* GetComponent(ECS::EntityId entityId);
    
    template<typename T>
    const T* GetComponent(ECS::EntityId entityId) const;
    
    template<typename T>
    void RemoveComponent(ECS::EntityId entityId);
    
    bool EntityHasComponent(ECS::EntityId entityId, ECS::ComponentTypeId typeId) const;
    
    // Callbacks
    void SetOnEntityCreated(EntityCallback cb);
    void SetOnEntityDestroyed(EntityCallback cb);
    void SetOnComponentAdded(ComponentCallback cb);
    void SetOnComponentRemoved(ComponentCallback cb);
    
    // Archetype
    ECS::ArchetypeManager& GetArchetypeManager() { return m_ArchetypeManager; }
    const ECS::ArchetypeManager& GetArchetypeManager() const { return m_ArchetypeManager; }
    
    // Thread Safety
    void Lock();
    void Unlock();
    
private:
    struct EntityInfo
    {
        ECS::EntityVersion version{0};
        std::vector<ECS::ComponentTypeId> components;
        bool alive{false};
    };
    
    std::unordered_map<ECS::EntityId, EntityInfo> m_Entities;
    std::unordered_map<ECS::ComponentTypeId, std::unique_ptr<ECS::ComponentArray>> m_ComponentArrays;
    ECS::ArchetypeManager m_ArchetypeManager;
    
    mutable std::mutex m_Mutex;
    ECS::EntityId m_NextEntityId{0};
    std::vector<ECS::EntityId> m_FreeEntityIds;
    
    EntityCallback m_OnEntityCreated;
    EntityCallback m_OnEntityDestroyed;
    ComponentCallback m_OnComponentAdded;
    ComponentCallback m_OnComponentRemoved;
    
    ECS::ComponentArray& GetComponentArray(ECS::ComponentTypeId typeId);
};

// Template Implementations

template<typename T>
void WorldState::AddComponent(ECS::EntityId entityId, const T& component)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive) {
        return;
    }
    
    ECS::ComponentTypeId typeId = ECS::GetComponentTypeId<T>();
    
    // Get or create component array
    if (m_ComponentArrays.find(typeId) == m_ComponentArrays.end()) {
        m_ComponentArrays[typeId] = std::make_unique<ECS::TypedComponentArray<T>>();
    }
    
    auto& array = static_cast<ECS::TypedComponentArray<T>&>(*m_ComponentArrays[typeId]);
    ECS::ComponentIndex index = array.AddComponent(component);
    
    it->second.components.push_back(typeId);
    
    // Update archetype
    m_ArchetypeManager.GetOrCreateArchetype(it->second.components);
    
    if (m_OnComponentAdded) {
        m_OnComponentAdded(entityId, typeId);
    }
}

template<typename T>
T* WorldState::GetComponent(ECS::EntityId entityId)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive) {
        return nullptr;
    }
    
    ECS::ComponentTypeId typeId = ECS::GetComponentTypeId<T>();
    auto compIt = m_ComponentArrays.find(typeId);
    if (compIt == m_ComponentArrays.end()) {
        return nullptr;
    }
    
    auto& array = static_cast<ECS::TypedComponentArray<T>&>(*compIt->second);
    // In production, you'd map entityId to component index
    return &array.GetComponent(0); // Simplified for now
}

template<typename T>
const T* WorldState::GetComponent(ECS::EntityId entityId) const
{
    return const_cast<WorldState*>(this)->GetComponent<T>(entityId);
}

template<typename T>
void WorldState::RemoveComponent(ECS::EntityId entityId)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive) {
        return;
    }
    
    ECS::ComponentTypeId typeId = ECS::GetComponentTypeId<T>();
    auto compIt = m_ComponentArrays.find(typeId);
    if (compIt == m_ComponentArrays.end()) {
        return;
    }
    
    auto& components = it->second.components;
    components.erase(
        std::remove(components.begin(), components.end(), typeId),
        components.end()
    );
    
    if (m_OnComponentRemoved) {
        m_OnComponentRemoved(entityId, typeId);
    }
}

} // namespace SagaEngine::Simulation