#pragma once
#include "Entity.h"
#include "Component.h"
#include <vector>
#include <functional>

namespace SagaEngine::Simulation { class WorldState; }

namespace SagaEngine::ECS
{

class Query
{
public:
    using FilterFunc = std::function<bool(EntityId)>;
    
    explicit Query(Simulation::WorldState* world);
    
    Query& WithComponent(ComponentTypeId type);
    Query& WithFilter(FilterFunc filter);
    
    std::vector<EntityId> Execute();
    
private:
    Simulation::WorldState* m_World;
    std::vector<ComponentTypeId> m_RequiredComponents;
    FilterFunc m_Filter;
};

} // namespace SagaEngine::ECS