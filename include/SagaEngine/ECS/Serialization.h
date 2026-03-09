#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <atomic>
#include <mutex>

namespace SagaEngine::ECS {

using ComponentTypeId = uint32_t;

class Serializer {
public:
    virtual ~Serializer() = default;
    virtual size_t Serialize(const void* data, void* buffer, size_t bufferSize) const = 0;
    virtual size_t Deserialize(void* data, const void* buffer, size_t bufferSize) const = 0;
    virtual size_t GetSerializedSize() const = 0;
};

template<typename T>
class TypedSerializer : public Serializer {
public:
    using SerializeFunc = std::function<size_t(const T&, void*, size_t)>;
    using DeserializeFunc = std::function<size_t(T&, const void*, size_t)>;
    using SizeFunc = std::function<size_t()>;

    TypedSerializer(SerializeFunc ser, DeserializeFunc deser, SizeFunc size)
        : _serialize(ser), _deserialize(deser), _getSize(size) {}

    size_t Serialize(const void* data, void* buffer, size_t bufferSize) const override {
        return _serialize(*static_cast<const T*>(data), buffer, bufferSize);
    }

    size_t Deserialize(void* data, const void* buffer, size_t bufferSize) const override {
        return _deserialize(*static_cast<T*>(data), buffer, bufferSize);
    }

    size_t GetSerializedSize() const override {
        return _getSize();
    }

private:
    SerializeFunc _serialize;
    DeserializeFunc _deserialize;
    SizeFunc _getSize;
};

class ComponentRegistry {
public:
    static ComponentRegistry& Instance();

    template<typename T>
    void RegisterComponent(const char* name, 
                          typename TypedSerializer<T>::SerializeFunc ser,
                          typename TypedSerializer<T>::DeserializeFunc deser,
                          typename TypedSerializer<T>::SizeFunc size);

    ComponentTypeId GetComponentTypeId(const char* name) const;
    const char* GetComponentName(ComponentTypeId id) const;
    const Serializer* GetSerializer(ComponentTypeId id) const;
    size_t GetComponentCount() const { return _components.size(); }

private:
    struct ComponentInfo {
        std::string name;
        ComponentTypeId id;
        std::unique_ptr<Serializer> serializer;
        std::type_index typeIndex;

        // ✅ Default constructor with type_index initialization
        ComponentInfo() 
            : id(0)
            , typeIndex(std::type_index(typeid(void))) {}

        ComponentInfo(std::string n, ComponentTypeId i, 
                     std::unique_ptr<Serializer> s, std::type_index t)
            : name(std::move(n))
            , id(i)
            , serializer(std::move(s))
            , typeIndex(t) {}
    };

    std::unordered_map<std::string, ComponentInfo> _components;
    std::unordered_map<ComponentTypeId, std::string> _idToName;
    std::atomic<ComponentTypeId> _nextId{0};
    mutable std::mutex _mutex;
};

template<typename T>
void ComponentRegistry::RegisterComponent(const char* name,
                                          typename TypedSerializer<T>::SerializeFunc ser,
                                          typename TypedSerializer<T>::DeserializeFunc deser,
                                          typename TypedSerializer<T>::SizeFunc size) {
    std::lock_guard<std::mutex> lock(_mutex);
    ComponentTypeId id = _nextId.fetch_add(1, std::memory_order_relaxed);
    
    _components[name] = ComponentInfo{
        name,
        id,
        std::make_unique<TypedSerializer<T>>(ser, deser, size),
        std::type_index(typeid(T))
    };
    _idToName[id] = name;
}

} // namespace SagaEngine::ECS