/// @file ComponentObserver.h
/// @brief Event-driven component lifecycle observers for the ECS.
///
/// Layer  : Engine / ECS
/// Purpose: Allows gameplay and engine systems to react to component additions,
///          removals, and modifications without polling.  Observers are registered
///          per-component-type and fire synchronously on the thread that performs
///          the mutation.
///
/// Integration:
///   - Archetype migration (ArchetypeMigrator) fires OnAdd / OnRemove.
///   - TypedComponentArray can fire OnModified if wrapped with a dirty setter.
///   - Replication dirty-marking hooks into OnModified to auto-mark components.
///
/// Threading: NOT inherently thread-safe.  If mutations happen from multiple
///            threads, external synchronisation or deferred dispatch is required.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace SagaEngine::ECS
{

// ─── Observer Events ──────────────────────────────────────────────────────────

/// Event kind for component lifecycle transitions.
enum class ComponentEvent : uint8_t
{
    Added,    ///< Component was attached to an entity.
    Removed,  ///< Component was detached from an entity.
    Modified  ///< Component data was changed in-place.
};

/// Convert ComponentEvent to a log-friendly string.
[[nodiscard]] inline const char* ComponentEventToString(ComponentEvent ev) noexcept
{
    switch (ev)
    {
        case ComponentEvent::Added:    return "Added";
        case ComponentEvent::Removed:  return "Removed";
        case ComponentEvent::Modified: return "Modified";
        default:                       return "Unknown";
    }
}

// ─── Observer Callback ────────────────────────────────────────────────────────

/// Signature for component observer callbacks.
///
/// @param entityId      The entity affected.
/// @param componentType The component type ID.
/// @param event         What happened (Added / Removed / Modified).
using ComponentObserverFn = std::function<void(EntityId        entityId,
                                               ComponentTypeId componentType,
                                               ComponentEvent  event)>;

// ─── Observer Handle ──────────────────────────────────────────────────────────

/// Opaque handle returned on registration, used to unregister.
using ObserverHandle = uint32_t;

static constexpr ObserverHandle kInvalidObserverHandle = 0;

// ─── Component Observer Registry ──────────────────────────────────────────────

/// Central registry for component lifecycle observers.
///
/// Usage:
///   auto handle = registry.Register(GetComponentTypeId<Health>(),
///       ComponentEvent::Modified,
///       [](EntityId id, ComponentTypeId type, ComponentEvent ev) {
///           // react to Health being modified on entity id
///       });
///
///   // ... later ...
///   registry.Unregister(handle);
class ComponentObserverRegistry final
{
public:
    ComponentObserverRegistry()  = default;
    ~ComponentObserverRegistry() = default;

    ComponentObserverRegistry(const ComponentObserverRegistry&)            = delete;
    ComponentObserverRegistry& operator=(const ComponentObserverRegistry&) = delete;

    // ── Registration ──────────────────────────────────────────────────────────

    /// Register an observer for a specific component type and event.
    /// Returns a handle that can be used to unregister.
    [[nodiscard]] ObserverHandle Register(ComponentTypeId    typeId,
                                          ComponentEvent     event,
                                          ComponentObserverFn callback);

    /// Register an observer for ALL component types and a specific event.
    [[nodiscard]] ObserverHandle RegisterGlobal(ComponentEvent     event,
                                                ComponentObserverFn callback);

    /// Remove a previously registered observer.
    void Unregister(ObserverHandle handle);

    /// Remove all observers.
    void Clear();

    // ── Dispatch ──────────────────────────────────────────────────────────────

    /// Fire all matching observers for a component event.
    /// Called by the ECS internals (ArchetypeMigrator, ComponentArray wrappers).
    void Notify(EntityId        entityId,
                ComponentTypeId typeId,
                ComponentEvent  event) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    /// Total number of registered observers.
    [[nodiscard]] std::size_t ObserverCount() const noexcept;

private:
    struct Entry
    {
        ObserverHandle      handle{kInvalidObserverHandle};
        ComponentTypeId     typeId{INVALID_COMPONENT_TYPE}; ///< INVALID = global
        ComponentEvent      event{ComponentEvent::Added};
        ComponentObserverFn callback;
    };

    /// Monotonically increasing handle counter.
    ObserverHandle m_nextHandle{1};

    /// All registered observers. Linear scan is acceptable for typical counts
    /// (< 100 observers); if profiling reveals a bottleneck, switch to a
    /// two-level map (typeId → vector<Entry>).
    std::vector<Entry> m_observers;
};

} // namespace SagaEngine::ECS
