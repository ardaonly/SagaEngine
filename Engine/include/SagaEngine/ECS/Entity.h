#pragma once
#include <cstdint>
#include <type_traits>

namespace SagaEngine::ECS
{

using EntityId = uint32_t;
using EntityVersion = uint16_t;

constexpr EntityId INVALID_ENTITY_ID = UINT32_MAX;

struct EntityHandle
{
    EntityId id{INVALID_ENTITY_ID};
    EntityVersion version{0};
    
    bool IsValid() const { return id != INVALID_ENTITY_ID; }
    bool operator==(const EntityHandle& other) const { return id == other.id && version == other.version; }
    bool operator!=(const EntityHandle& other) const { return !(*this == other); }
};

class Entity
{
public:
    Entity();
    explicit Entity(EntityHandle handle);
    
    EntityHandle GetHandle() const { return m_Handle; }
    EntityId GetId() const { return m_Handle.id; }
    EntityVersion GetVersion() const { return m_Handle.version; }
    bool IsValid() const { return m_Handle.IsValid(); }
    void Invalidate();
    
private:
    EntityHandle m_Handle;
};

} // namespace SagaEngine::ECS