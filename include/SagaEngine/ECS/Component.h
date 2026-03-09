#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <type_traits>

namespace SagaEngine::ECS
{

using ComponentTypeId = uint32_t;
using ComponentIndex = uint32_t;

constexpr ComponentTypeId INVALID_COMPONENT_TYPE = UINT32_MAX;

template<typename T>
inline ComponentTypeId GetComponentTypeId()
{
    static ComponentTypeId s_Id = []() {
        static ComponentTypeId s_NextId = 0;
        return s_NextId++;
    }();
    return s_Id;
}

class ComponentArray
{
public:
    virtual ComponentTypeId GetTypeId() const = 0;
    virtual size_t GetCount() const = 0;
    virtual void RemoveComponent(ComponentIndex index) = 0;
    virtual void Compact() = 0;
};

template<typename T>
class TypedComponentArray : public ComponentArray
{
public:
    TypedComponentArray() = default;
    
    ComponentTypeId GetTypeId() const override { return GetComponentTypeId<T>(); }
    size_t GetCount() const override { return m_Data.size(); }
    
    ComponentIndex AddComponent(const T& component)
    {
        m_Data.push_back(component);
        m_Version.push_back(1);
        return static_cast<ComponentIndex>(m_Data.size() - 1);
    }
    
    T& GetComponent(ComponentIndex index) { return m_Data[index]; }
    const T& GetComponent(ComponentIndex index) const { return m_Data[index]; }
    
    void RemoveComponent(ComponentIndex index) override
    {
        if (index >= m_Data.size()) return;
        
        m_Data[index] = m_Data.back();
        m_Version[index] = m_Version.back();
        m_Data.pop_back();
        m_Version.pop_back();
    }
    
    void Compact() override
    {
        // Already compact due to swap-remove
    }
    
    uint16_t GetComponentVersion(ComponentIndex index) const
    {
        return index < m_Version.size() ? m_Version[index] : 0;
    }
    
private:
    std::vector<T> m_Data;
    std::vector<uint16_t> m_Version;
};

} // namespace SagaEngine::ECS