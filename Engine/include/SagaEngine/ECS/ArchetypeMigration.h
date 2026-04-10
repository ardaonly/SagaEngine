/// @file ArchetypeMigration.h
/// @brief Runtime archetype migration — add / remove components on live entities.
///
/// Layer  : Engine / ECS
/// Purpose: When a component is added to or removed from an entity, the entity
///          must move from its current archetype to a new one that matches the
///          updated component set.  This is called "archetype migration".
///
/// Design:
///   - ArchetypeMigrator operates on the existing ArchetypeManager.
///   - Migration is a two-step process:
///       1. Determine the target archetype (current ± component type).
///       2. Move the entity's data from the source archetype's storage to
///          the target archetype's storage.
///   - ComponentArrays are moved by type; the new component is default-
///     constructed (AddComponent) or dropped (RemoveComponent).
///
/// Threading: NOT thread-safe.  Must be called from the main simulation thread
///            or guarded externally.  Deferred migration (batched at frame end)
///            is recommended for parallel ECS systems.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/Archetype.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace SagaEngine::ECS
{

// ─── Migration Request ────────────────────────────────────────────────────────

/// Describes a pending migration operation that can be deferred and batched.
struct MigrationRequest
{
    EntityId        entityId{INVALID_ENTITY_ID};
    ComponentTypeId addType{INVALID_COMPONENT_TYPE};    ///< Type to add (or INVALID if remove).
    ComponentTypeId removeType{INVALID_COMPONENT_TYPE}; ///< Type to remove (or INVALID if add).
};

// ─── Migration Result ─────────────────────────────────────────────────────────

/// Outcome of a single migration attempt.
enum class MigrationResult : uint8_t
{
    Success,                ///< Entity migrated successfully.
    EntityNotFound,         ///< Entity does not belong to any archetype.
    AlreadyHasComponent,    ///< AddComponent: entity already has this type.
    DoesNotHaveComponent,   ///< RemoveComponent: entity does not have this type.
    ArchetypeCreationFailed ///< Internal error creating the target archetype.
};

/// Convert MigrationResult to a log-friendly string.
[[nodiscard]] inline const char* MigrationResultToString(MigrationResult r) noexcept
{
    switch (r)
    {
        case MigrationResult::Success:                return "Success";
        case MigrationResult::EntityNotFound:         return "EntityNotFound";
        case MigrationResult::AlreadyHasComponent:    return "AlreadyHasComponent";
        case MigrationResult::DoesNotHaveComponent:   return "DoesNotHaveComponent";
        case MigrationResult::ArchetypeCreationFailed:return "ArchetypeCreationFailed";
        default:                                      return "Unknown";
    }
}

// ─── Migration Observer ───────────────────────────────────────────────────────

/// Callback fired after a successful migration.
/// Useful for replication dirty-marking, observer notifications, etc.
using OnMigrationFn = std::function<void(EntityId          entityId,
                                         const Archetype&  oldArchetype,
                                         const Archetype&  newArchetype)>;

// ─── Archetype Migrator ───────────────────────────────────────────────────────

/// Stateless utility that performs archetype migrations on an ArchetypeManager.
///
/// Usage (immediate):
///   ArchetypeMigrator migrator(&archetypeManager, &entityArchetypeMap);
///   migrator.AddComponent<Health>(entityId, Health{100});
///
/// Usage (deferred):
///   migrator.EnqueueAdd(entityId, GetComponentTypeId<Health>());
///   migrator.FlushDeferred();
class ArchetypeMigrator final
{
public:
    /// `entityArchetypes` maps each EntityId to the archetype it currently
    /// belongs to.  The migrator updates this map on successful migration.
    ArchetypeMigrator(ArchetypeManager*                                  manager,
                      std::unordered_map<EntityId, Archetype*>*          entityArchetypes);

    ~ArchetypeMigrator() = default;

    ArchetypeMigrator(const ArchetypeMigrator&)            = delete;
    ArchetypeMigrator& operator=(const ArchetypeMigrator&) = delete;

    // ── Immediate migration ───────────────────────────────────────────────────

    /// Add a component type to an entity, migrating it to the new archetype.
    [[nodiscard]] MigrationResult AddComponent(EntityId entityId, ComponentTypeId typeId);

    /// Remove a component type from an entity, migrating it to the new archetype.
    [[nodiscard]] MigrationResult RemoveComponent(EntityId entityId, ComponentTypeId typeId);

    // ── Deferred migration ────────────────────────────────────────────────────

    /// Enqueue an add-component migration to be flushed later.
    void EnqueueAdd(EntityId entityId, ComponentTypeId typeId);

    /// Enqueue a remove-component migration to be flushed later.
    void EnqueueRemove(EntityId entityId, ComponentTypeId typeId);

    /// Execute all deferred migrations in submission order.
    /// Returns the number of successful migrations.
    [[nodiscard]] uint32_t FlushDeferred();

    /// Discard all pending deferred migrations.
    void ClearDeferred() noexcept;

    /// Return the number of pending deferred requests.
    [[nodiscard]] std::size_t DeferredCount() const noexcept { return m_deferred.size(); }

    // ── Observer ──────────────────────────────────────────────────────────────

    /// Register a callback fired after each successful migration.
    void SetOnMigration(OnMigrationFn fn) { m_onMigration = std::move(fn); }

private:
    /// Resolve the target archetype by adding/removing a type from the source.
    [[nodiscard]] Archetype* ResolveTarget(const Archetype& source,
                                           ComponentTypeId  addType,
                                           ComponentTypeId  removeType);

    /// Move entity data from source to target archetype.
    void MoveEntity(EntityId entityId, Archetype& source, Archetype& target);

    ArchetypeManager*                                 m_manager;
    std::unordered_map<EntityId, Archetype*>*         m_entityArchetypes;
    std::vector<MigrationRequest>                     m_deferred;
    OnMigrationFn                                     m_onMigration;
};

} // namespace SagaEngine::ECS
