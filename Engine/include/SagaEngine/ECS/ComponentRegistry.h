/// @file ComponentRegistry.h
/// @brief Deterministic component type ID registry for cross-binary ECS serialization.
///
/// Problem solved: GetComponentTypeId<T>() uses an atomic runtime counter whose
/// value depends on static-initialization order and link order. Two separately
/// compiled binaries (client + server) may assign different IDs to the same type,
/// breaking WorldState serialization and replication.
///
/// Solution: every component type used in WorldState must be registered with an
/// explicit, hard-coded uint32_t before any WorldState is created.
/// ComponentRegistry is the single source of truth for those IDs.
///
/// Usage pattern (one registration .cpp per project):
///   SAGA_REGISTER_COMPONENT(TransformComponent, 1);
///   SAGA_REGISTER_COMPONENT(HealthComponent,    2);
///   SAGA_REGISTER_COMPONENT(VelocityComponent,  3);
///
/// Thread safety: Register() is mutex-protected for safe concurrent startup.
///               GetId() is read-only after startup — safe for concurrent reads.

#pragma once

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/Core/Log/Log.h"

#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>

namespace SagaEngine::ECS {

/// Deterministic, explicit component type ID registry.
///
/// All component types used in WorldState must be registered once at startup
/// via Register<T>() before any simulation begins. Attempting to retrieve the
/// ID of an unregistered type is a hard error — it surfaces the bug immediately
/// rather than silently producing a wrong ID.
class ComponentRegistry
{
public:
    /// Return the process-wide singleton registry.
    [[nodiscard]] static ComponentRegistry& Instance() noexcept;

    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    // ─── Registration ─────────────────────────────────────────────────────────

    /// Register component type T with a stable, explicit ID.
    ///
    /// @tparam T    Component struct. Must be trivially copyable for serialization.
    /// @param  id   Stable ID, unique across all registered types.
    /// @param  name Human-readable name for logs and diagnostics.
    /// @throws std::logic_error on ID or type collision.
    template<typename T>
    void Register(ComponentTypeId id, std::string_view name);

    // ─── Lookup ───────────────────────────────────────────────────────────────

    /// Return the registered ID for component type T.
    ///
    /// @throws std::logic_error if T has not been registered.
    template<typename T>
    [[nodiscard]] ComponentTypeId GetId() const;

    /// Return the registered ID for a runtime type_index.
    ///
    /// @throws std::logic_error if the type has not been registered.
    [[nodiscard]] ComponentTypeId GetId(std::type_index type) const;

    /// Return the human-readable name for a type ID, or "<unknown>".
    [[nodiscard]] std::string_view GetName(ComponentTypeId id) const noexcept;

    // ─── Predicates ───────────────────────────────────────────────────────────

    /// Return true if type T has been registered.
    template<typename T>
    [[nodiscard]] bool IsRegistered() const noexcept;

    /// Return true if the given numeric ID is already taken.
    [[nodiscard]] bool IsIdTaken(ComponentTypeId id) const noexcept;

    // ─── Maintenance ──────────────────────────────────────────────────────────

    /// Remove all registrations. Intended for unit-test teardown only.
    void Reset() noexcept;

private:
    ComponentRegistry() = default;

    mutable std::mutex                                    m_mutex;
    std::unordered_map<std::type_index, ComponentTypeId>  m_typeToId; ///< type → stable ID
    std::unordered_map<ComponentTypeId, std::type_index>  m_idToType; ///< stable ID → type
    std::unordered_map<ComponentTypeId, std::string>      m_idToName; ///< stable ID → debug name
};

// ─── Template implementations ─────────────────────────────────────────────────

template<typename T>
void ComponentRegistry::Register(ComponentTypeId id, std::string_view name)
{
    static_assert(std::is_trivially_copyable_v<T>,
        "WorldState components must be trivially copyable for deterministic serialization.");

    std::lock_guard lock(m_mutex);

    const std::type_index ti(typeid(T));

    if (m_typeToId.contains(ti))
    {
        LOG_ERROR("ComponentRegistry",
            "Type '%.*s' already registered (existing id=%u).",
            static_cast<int>(name.size()), name.data(),
            m_typeToId.at(ti));
        throw std::logic_error("ComponentRegistry: type already registered");
    }

    if (m_idToType.contains(id))
    {
        LOG_ERROR("ComponentRegistry",
            "ID %u already taken by '%s', cannot register '%.*s'.",
            id, m_idToName.at(id).c_str(),
            static_cast<int>(name.size()), name.data());
        throw std::logic_error("ComponentRegistry: ID collision");
    }

    m_typeToId.emplace(ti, id);
    m_idToType.emplace(id, ti);
    m_idToName.emplace(id, std::string(name));

    LOG_INFO("ComponentRegistry", "Registered '%.*s' → id=%u",
             static_cast<int>(name.size()), name.data(), id);
}

template<typename T>
ComponentTypeId ComponentRegistry::GetId() const
{
    const std::type_index ti(typeid(T));
    std::lock_guard lock(m_mutex);

    const auto it = m_typeToId.find(ti);
    if (it == m_typeToId.end())
    {
        LOG_ERROR("ComponentRegistry",
            "Unregistered component type: %s. "
            "Call SAGA_REGISTER_COMPONENT at startup.", typeid(T).name());
        throw std::logic_error("ComponentRegistry: type not registered");
    }
    return it->second;
}

template<typename T>
bool ComponentRegistry::IsRegistered() const noexcept
{
    const std::type_index ti(typeid(T));
    std::lock_guard lock(m_mutex);
    return m_typeToId.contains(ti);
}

} // namespace SagaEngine::ECS

// ─── Registration macro ───────────────────────────────────────────────────────

/// Register a component type with a stable ID.
///
/// Place calls in a dedicated ComponentRegistration.cpp, executed before
/// any WorldState is constructed.
///
/// Example:
///   SAGA_REGISTER_COMPONENT(TransformComponent, 1);
///   SAGA_REGISTER_COMPONENT(HealthComponent,    2);
#define SAGA_REGISTER_COMPONENT(Type, Id) \
    ::SagaEngine::ECS::ComponentRegistry::Instance().Register<Type>((Id), #Type)
