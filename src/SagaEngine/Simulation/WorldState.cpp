#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Simulation
{

WorldState::WorldState()
{
    LOG_INFO("WorldState", "WorldState initialized");
}

WorldState::~WorldState()
{
    LOG_INFO("WorldState", "WorldState shutdown");
}

ECS::Entity WorldState::CreateEntity()
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    ECS::EntityId id;
    if (!m_FreeEntityIds.empty()) {
        id = m_FreeEntityIds.back();
        m_FreeEntityIds.pop_back();
    } else {
        id = m_NextEntityId++;
    }
    
    EntityInfo info;
    info.version = 1;
    info.alive = true;
    m_Entities[id] = info;
    
    ECS::EntityHandle handle{id, info.version};
    ECS::Entity entity(handle);
    
    if (m_OnEntityCreated) {
        m_OnEntityCreated(id);
    }
    
    LOG_DEBUG("WorldState", "Created entity: %u", id);
    return entity;
}

void WorldState::DestroyEntity(ECS::EntityId entityId)
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive) {
        return;
    }
    
    // Remove all components
    for (auto typeId : it->second.components) {
        auto compIt = m_ComponentArrays.find(typeId);
        if (compIt != m_ComponentArrays.end()) {
            compIt->second->RemoveComponent(0); // Simplified
        }
    }
    
    it->second.alive = false;
    it->second.version++;
    m_FreeEntityIds.push_back(entityId);
    
    if (m_OnEntityDestroyed) {
        m_OnEntityDestroyed(entityId);
    }
    
    LOG_DEBUG("WorldState", "Destroyed entity: %u", entityId);
}

bool WorldState::IsEntityAlive(ECS::EntityId entityId) const
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    return it != m_Entities.end() && it->second.alive;
}

std::vector<ECS::EntityId> WorldState::GetAllEntities() const
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    std::vector<ECS::EntityId> result;
    result.reserve(m_Entities.size());
    
    for (const auto& [id, info] : m_Entities) {
        if (info.alive) {
            result.push_back(id);
        }
    }
    
    return result;
}

bool WorldState::EntityHasComponent(ECS::EntityId entityId, ECS::ComponentTypeId typeId) const
{
    std::lock_guard<std::mutex> lk(m_Mutex);
    
    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive) {
        return false;
    }
    
    return std::find(it->second.components.begin(), it->second.components.end(), typeId) 
           != it->second.components.end();
}

void WorldState::SetOnEntityCreated(EntityCallback cb)
{
    m_OnEntityCreated = std::move(cb);
}

void WorldState::SetOnEntityDestroyed(EntityCallback cb)
{
    m_OnEntityDestroyed = std::move(cb);
}

void WorldState::SetOnComponentAdded(ComponentCallback cb)
{
    m_OnComponentAdded = std::move(cb);
}

void WorldState::SetOnComponentRemoved(ComponentCallback cb)
{
    m_OnComponentRemoved = std::move(cb);
}

void WorldState::Lock()
{
    m_Mutex.lock();
}

void WorldState::Unlock()
{
    m_Mutex.unlock();
}

ECS::ComponentArray& WorldState::GetComponentArray(ECS::ComponentTypeId typeId)
{
    auto it = m_ComponentArrays.find(typeId);
    if (it == m_ComponentArrays.end()) {
        LOG_ERROR("WorldState", "Component array not found: %u", typeId);
        static ECS::TypedComponentArray<int> s_Dummy;
        return s_Dummy;
    }
    return *it->second;
}

} // namespace SagaEngine::Simulation