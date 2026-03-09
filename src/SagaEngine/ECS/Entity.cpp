#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::ECS
{

Entity::Entity()
    : m_Handle{INVALID_ENTITY_ID, 0}
{
}

Entity::Entity(EntityHandle handle)
    : m_Handle(handle)
{
}

void Entity::Invalidate()
{
    m_Handle.id = INVALID_ENTITY_ID;
    m_Handle.version = 0;
}

} // namespace SagaEngine::ECS