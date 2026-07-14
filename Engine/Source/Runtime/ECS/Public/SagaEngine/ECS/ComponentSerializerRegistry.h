#pragma once

/// @file ComponentSerializerRegistry.h
/// @brief Deterministic component serialization registry with explicit type IDs.
///
/// Component type IDs must be identical across all binaries (client, server,
/// tools). This registry enforces explicit ID assignment — no auto-increment,
/// no implicit ordering dependency. Two binaries that register the same
/// component with the same ID will serialize/deserialize compatibly.

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

/// Serialize / deserialize interface for a single component type.
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

/// Cross-binary component serializer registry.
///
/// Every component type is registered with an explicit, deterministic
/// ComponentTypeId that must match across client, server, and tool binaries.
/// Registration order does not affect ID assignment — the caller supplies
/// the ID, and collisions are rejected.
class ComponentSerializerRegistry {
public:
    static ComponentSerializerRegistry& Instance();

    /// Register a component type with an explicit, deterministic ID.
    /// @param id   Must match the same value in every binary that serializes
    ///             this component. Collisions throw std::logic_error.
    /// @param name Human-readable identifier (used for diagnostics only).
    /// @param ser    Serialization lambda for this component type.
    /// @param deser  Deserialization lambda for this component type.
    /// @param size   Size-of-serialized-form lambda for this component type.
    template<typename T>
    void Register(ComponentTypeId id,
                  const char* name,
                  typename TypedSerializer<T>::SerializeFunc ser,
                  typename TypedSerializer<T>::DeserializeFunc deser,
                  typename TypedSerializer<T>::SizeFunc size);

    /// Look up the type ID for a registered component by name.
    /// Returns UINT32_MAX if not found.
    [[nodiscard]] ComponentTypeId GetComponentTypeId(const char* name) const;

    /// Look up the name for a registered component by ID.
    /// Returns nullptr if not found.
    [[nodiscard]] const char* GetComponentName(ComponentTypeId id) const;

    /// Get the serializer for a component type ID.
    /// Returns nullptr if not found.
    [[nodiscard]] const Serializer* GetSerializer(ComponentTypeId id) const;

    /// Number of registered component types.
    [[nodiscard]] size_t GetComponentCount() const { return _components.size(); }

    /// Clear all registrations (for test teardown only).
    void Reset();

private:
    struct ComponentInfo {
        std::string name;
        ComponentTypeId id;
        std::unique_ptr<Serializer> serializer;
        std::type_index typeIndex;

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
    mutable std::mutex _mutex;
};

template<typename T>
void ComponentSerializerRegistry::Register(ComponentTypeId id,
                                           const char* name,
                                           typename TypedSerializer<T>::SerializeFunc ser,
                                           typename TypedSerializer<T>::DeserializeFunc deser,
                                           typename TypedSerializer<T>::SizeFunc size) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (id == UINT32_MAX) {
        throw std::logic_error(
            "ComponentSerializerRegistry::Register: ComponentTypeId UINT32_MAX is reserved.");
    }

    if (_idToName.find(id) != _idToName.end()) {
        throw std::logic_error(
            ("ComponentSerializerRegistry::Register: ID collision on " +
             std::to_string(id)).c_str());
    }

    _components[name] = ComponentInfo{
        name,
        id,
        std::make_unique<TypedSerializer<T>>(ser, deser, size),
        std::type_index(typeid(T))
    };
    _idToName[id] = name;
}

} // namespace SagaEngine::ECS