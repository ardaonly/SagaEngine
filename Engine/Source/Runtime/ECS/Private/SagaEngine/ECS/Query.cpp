#include "SagaEngine/ECS/Query.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::ECS
{

Query::Query(Simulation::WorldState* world)
    : m_World(world)
{
}

Query& Query::WithComponent(ComponentTypeId type)
{
    m_RequiredComponents.push_back(type);
    return *this;
}

Query& Query::WithFilter(FilterFunc filter)
{
    m_Filter = std::move(filter);
    return *this;
}

std::vector<EntityId> Query::Execute()
{
    std::vector<EntityId> result;

    if (!m_World)
    {
        LOG_ERROR("ECS::Query", "WorldState is null");
        return result;
    }

    auto entities = m_World->Query(m_RequiredComponents);

    for (EntityId entityId : entities)
    {
        if (m_Filter && !m_Filter(entityId))
            continue;

        result.push_back(entityId);
    }

    return result;
}

} // namespace SagaEngine::ECS