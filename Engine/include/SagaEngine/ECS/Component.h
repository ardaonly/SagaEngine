#pragma once
#include "SagaEngine/ECS/Entity.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <atomic>

namespace SagaEngine::ECS
{

using ComponentTypeId = uint32_t;
using ComponentIndex  = uint32_t;

constexpr ComponentTypeId INVALID_COMPONENT_TYPE = UINT32_MAX;

namespace detail
{
    inline std::atomic<ComponentTypeId>& GlobalComponentCounter()
    {
        static std::atomic<ComponentTypeId> s_Counter{0};
        return s_Counter;
    }
}

template<typename T>
inline ComponentTypeId GetComponentTypeId()
{
    static const ComponentTypeId s_Id =
        detail::GlobalComponentCounter().fetch_add(1, std::memory_order_relaxed);
    return s_Id;
}

class ComponentArray
{
public:
    virtual ~ComponentArray() = default;
    virtual ComponentTypeId GetTypeId()                                          const = 0;
    virtual size_t          GetCount()                                           const = 0;
    virtual EntityId        GetEntityAtIndex(ComponentIndex index)               const = 0;
    virtual void            RemoveComponent(EntityId entityId)                         = 0;
    virtual void            Compact()                                                  = 0;
};

template<typename T>
class TypedComponentArray : public ComponentArray
{
public:
    TypedComponentArray() = default;

    ComponentTypeId GetTypeId()  const override { return GetComponentTypeId<T>(); }
    size_t          GetCount()   const override { return m_Data.size(); }

    EntityId GetEntityAtIndex(ComponentIndex index) const override
    {
        return index < m_EntityIds.size() ? m_EntityIds[index] : INVALID_ENTITY_ID;
    }

    ComponentIndex AddComponent(EntityId entityId, const T& component)
    {
        ComponentIndex index = static_cast<ComponentIndex>(m_Data.size());
        m_Data.push_back(component);
        m_Version.push_back(1);
        m_EntityIds.push_back(entityId);
        m_IndexOf[entityId] = index;
        return index;
    }

    T* GetComponentForEntity(EntityId entityId)
    {
        auto it = m_IndexOf.find(entityId);
        if (it == m_IndexOf.end()) return nullptr;
        return &m_Data[it->second];
    }

    const T* GetComponentForEntity(EntityId entityId) const
    {
        auto it = m_IndexOf.find(entityId);
        if (it == m_IndexOf.end()) return nullptr;
        return &m_Data[it->second];
    }

    T& GetComponent(ComponentIndex index) { return m_Data[index]; }
    const T& GetComponent(ComponentIndex index) const { return m_Data[index]; }

    void RemoveComponent(EntityId entityId) override
    {
        auto it = m_IndexOf.find(entityId);
        if (it == m_IndexOf.end()) return;

        ComponentIndex removeIdx = it->second;
        ComponentIndex lastIdx   = static_cast<ComponentIndex>(m_Data.size() - 1);

        if (removeIdx != lastIdx)
        {
            EntityId movedEntity = m_EntityIds[lastIdx];

            m_Data[removeIdx]      = std::move(m_Data[lastIdx]);
            m_Version[removeIdx]   = m_Version[lastIdx];
            m_EntityIds[removeIdx] = movedEntity;
            m_IndexOf[movedEntity] = removeIdx;
        }

        m_Data.pop_back();
        m_Version.pop_back();
        m_EntityIds.pop_back();
        m_IndexOf.erase(it);
    }

    void Compact() override {}

    uint16_t GetComponentVersion(ComponentIndex index) const
    {
        return index < m_Version.size() ? m_Version[index] : 0;
    }

    bool HasEntity(EntityId entityId) const
    {
        return m_IndexOf.count(entityId) > 0;
    }

private:
    std::vector<T>              m_Data;
    std::vector<uint16_t>       m_Version;
    std::vector<EntityId>       m_EntityIds;
    std::unordered_map<EntityId, ComponentIndex> m_IndexOf;
};

} // namespace SagaEngine::ECS