#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/ECS/Serialization.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Simulation
{

WorldState::WorldState()
{
    LOG_INFO("WorldState", "Initialized");
}

WorldState::~WorldState()
{
    LOG_INFO("WorldState", "Shutdown");
}

ECS::Entity WorldState::CreateEntity()
{
    std::lock_guard<std::mutex> lk(m_Mutex);

    ECS::EntityId id;
    if (!m_FreeEntityIds.empty())
    {
        id = m_FreeEntityIds.back();
        m_FreeEntityIds.pop_back();
    }
    else
    {
        id = m_NextEntityId++;
    }

    EntityInfo info;
    info.version = 1;
    info.alive   = true;
    m_Entities[id] = std::move(info);

    ECS::EntityHandle handle{id, m_Entities[id].version};
    ECS::Entity entity(handle);

    if (m_OnEntityCreated)
        m_OnEntityCreated(id);

    LOG_DEBUG("WorldState", "Created entity %u", id);
    return entity;
}

void WorldState::DestroyEntity(ECS::EntityId entityId)
{
    std::lock_guard<std::mutex> lk(m_Mutex);

    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive)
        return;

    for (ECS::ComponentTypeId typeId : it->second.components)
    {
        auto compIt = m_ComponentArrays.find(typeId);
        if (compIt != m_ComponentArrays.end())
            compIt->second->RemoveComponent(entityId);
    }

    it->second.components.clear();
    it->second.alive = false;
    it->second.version++;
    m_FreeEntityIds.push_back(entityId);

    if (m_OnEntityDestroyed)
        m_OnEntityDestroyed(entityId);

    LOG_DEBUG("WorldState", "Destroyed entity %u", entityId);
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
    for (const auto& [id, info] : m_Entities)
    {
        if (info.alive)
            result.push_back(id);
    }
    return result;
}

bool WorldState::EntityHasComponent(ECS::EntityId entityId, ECS::ComponentTypeId typeId) const
{
    std::lock_guard<std::mutex> lk(m_Mutex);

    auto it = m_Entities.find(entityId);
    if (it == m_Entities.end() || !it->second.alive)
        return false;

    const auto& comps = it->second.components;
    return std::find(comps.begin(), comps.end(), typeId) != comps.end();
}

size_t WorldState::GetSerializedComponent(ECS::EntityId      entityId,
                                           ECS::ComponentTypeId typeId,
                                           uint8_t*           buf,
                                           size_t             bufSize) const
{
    std::lock_guard<std::mutex> lk(m_Mutex);

    auto entityIt = m_Entities.find(entityId);
    if (entityIt == m_Entities.end() || !entityIt->second.alive)
        return 0;

    auto compIt = m_ComponentArrays.find(typeId);
    if (compIt == m_ComponentArrays.end())
        return 0;

    const ECS::Serializer* ser =
        ECS::ComponentRegistry::Instance().GetSerializer(typeId);
    if (!ser)
        return 0;

    const ECS::ComponentArray& array = *compIt->second;
    for (ECS::ComponentIndex i = 0; i < static_cast<ECS::ComponentIndex>(array.GetCount()); ++i)
    {
        if (array.GetEntityAtIndex(i) == entityId)
        {
            const void* raw = nullptr;
            if (auto* typed = dynamic_cast<const ECS::ComponentArray*>(&array))
            {
                (void)typed;
            }
            return ser->Serialize(&array, buf, bufSize);
        }
    }
    return 0;
}

void WorldState::SetOnEntityCreated(EntityCallback cb)    { m_OnEntityCreated   = std::move(cb); }
void WorldState::SetOnEntityDestroyed(EntityCallback cb)  { m_OnEntityDestroyed = std::move(cb); }
void WorldState::SetOnComponentAdded(ComponentCallback cb){ m_OnComponentAdded  = std::move(cb); }
void WorldState::SetOnComponentRemoved(ComponentCallback cb){ m_OnComponentRemoved = std::move(cb); }

void WorldState::Lock()   { m_Mutex.lock(); }
void WorldState::Unlock() { m_Mutex.unlock(); }

} // namespace SagaEngine::Simulation