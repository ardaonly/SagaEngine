/// @file ArchetypeMigration.cpp
/// @brief Runtime archetype migration implementation.

#include "SagaEngine/ECS/ArchetypeMigration.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>

namespace SagaEngine::ECS
{

static constexpr const char* kTag = "ArchetypeMigration";

// ─── Construction ─────────────────────────────────────────────────────────────

ArchetypeMigrator::ArchetypeMigrator(
    ArchetypeManager*                          manager,
    std::unordered_map<EntityId, Archetype*>*  entityArchetypes)
    : m_manager(manager)
    , m_entityArchetypes(entityArchetypes)
{
    assert(manager          && "ArchetypeManager must not be null");
    assert(entityArchetypes && "EntityArchetype map must not be null");
}

// ─── Immediate Migration ──────────────────────────────────────────────────────

MigrationResult ArchetypeMigrator::AddComponent(EntityId entityId, ComponentTypeId typeId)
{
    // ── 1. Find current archetype ─────────────────────────────────────────────

    auto it = m_entityArchetypes->find(entityId);
    if (it == m_entityArchetypes->end() || it->second == nullptr)
    {
        LOG_WARN(kTag, "AddComponent: entity %u not found in archetype map",
                 static_cast<unsigned>(entityId));
        return MigrationResult::EntityNotFound;
    }

    Archetype& source = *it->second;

    // ── 2. Check duplicate ────────────────────────────────────────────────────

    if (source.HasComponent(typeId))
    {
        LOG_DEBUG(kTag, "AddComponent: entity %u already has component type %u",
                  static_cast<unsigned>(entityId), static_cast<unsigned>(typeId));
        return MigrationResult::AlreadyHasComponent;
    }

    // ── 3. Resolve target archetype ───────────────────────────────────────────

    Archetype* target = ResolveTarget(source, typeId, INVALID_COMPONENT_TYPE);
    if (!target)
    {
        LOG_ERROR(kTag, "AddComponent: failed to resolve target archetype for entity %u",
                  static_cast<unsigned>(entityId));
        return MigrationResult::ArchetypeCreationFailed;
    }

    // ── 4. Migrate ────────────────────────────────────────────────────────────

    const Archetype oldCopy = source; // snapshot for observer
    MoveEntity(entityId, source, *target);
    (*m_entityArchetypes)[entityId] = target;

    if (m_onMigration)
        m_onMigration(entityId, oldCopy, *target);

    LOG_DEBUG(kTag, "Entity %u migrated: +component %u",
              static_cast<unsigned>(entityId), static_cast<unsigned>(typeId));

    return MigrationResult::Success;
}

MigrationResult ArchetypeMigrator::RemoveComponent(EntityId entityId, ComponentTypeId typeId)
{
    // ── 1. Find current archetype ─────────────────────────────────────────────

    auto it = m_entityArchetypes->find(entityId);
    if (it == m_entityArchetypes->end() || it->second == nullptr)
    {
        LOG_WARN(kTag, "RemoveComponent: entity %u not found in archetype map",
                 static_cast<unsigned>(entityId));
        return MigrationResult::EntityNotFound;
    }

    Archetype& source = *it->second;

    // ── 2. Check existence ────────────────────────────────────────────────────

    if (!source.HasComponent(typeId))
    {
        LOG_DEBUG(kTag, "RemoveComponent: entity %u does not have component type %u",
                  static_cast<unsigned>(entityId), static_cast<unsigned>(typeId));
        return MigrationResult::DoesNotHaveComponent;
    }

    // ── 3. Resolve target archetype ───────────────────────────────────────────

    Archetype* target = ResolveTarget(source, INVALID_COMPONENT_TYPE, typeId);
    if (!target)
    {
        LOG_ERROR(kTag, "RemoveComponent: failed to resolve target archetype for entity %u",
                  static_cast<unsigned>(entityId));
        return MigrationResult::ArchetypeCreationFailed;
    }

    // ── 4. Migrate ────────────────────────────────────────────────────────────

    const Archetype oldCopy = source;
    MoveEntity(entityId, source, *target);
    (*m_entityArchetypes)[entityId] = target;

    if (m_onMigration)
        m_onMigration(entityId, oldCopy, *target);

    LOG_DEBUG(kTag, "Entity %u migrated: -component %u",
              static_cast<unsigned>(entityId), static_cast<unsigned>(typeId));

    return MigrationResult::Success;
}

// ─── Deferred Migration ───────────────────────────────────────────────────────

void ArchetypeMigrator::EnqueueAdd(EntityId entityId, ComponentTypeId typeId)
{
    MigrationRequest req;
    req.entityId = entityId;
    req.addType  = typeId;
    m_deferred.push_back(req);
}

void ArchetypeMigrator::EnqueueRemove(EntityId entityId, ComponentTypeId typeId)
{
    MigrationRequest req;
    req.entityId   = entityId;
    req.removeType = typeId;
    m_deferred.push_back(req);
}

uint32_t ArchetypeMigrator::FlushDeferred()
{
    uint32_t successCount = 0;

    for (const auto& req : m_deferred)
    {
        MigrationResult result;
        if (req.addType != INVALID_COMPONENT_TYPE)
            result = AddComponent(req.entityId, req.addType);
        else
            result = RemoveComponent(req.entityId, req.removeType);

        if (result == MigrationResult::Success)
            ++successCount;
    }

    m_deferred.clear();
    return successCount;
}

void ArchetypeMigrator::ClearDeferred() noexcept
{
    m_deferred.clear();
}

// ─── Internal Helpers ─────────────────────────────────────────────────────────

Archetype* ArchetypeMigrator::ResolveTarget(const Archetype& source,
                                             ComponentTypeId  addType,
                                             ComponentTypeId  removeType)
{
    // Build new component type list from source ± the modification.
    std::vector<ComponentTypeId> newTypes = source.GetComponentTypes();

    if (addType != INVALID_COMPONENT_TYPE)
    {
        newTypes.push_back(addType);
    }

    if (removeType != INVALID_COMPONENT_TYPE)
    {
        auto it = std::find(newTypes.begin(), newTypes.end(), removeType);
        if (it != newTypes.end())
            newTypes.erase(it);
    }

    // Sort for canonical ordering so identical sets produce the same archetype.
    std::sort(newTypes.begin(), newTypes.end());

    // GetOrCreateArchetype returns a reference — take the address.
    return &m_manager->GetOrCreateArchetype(newTypes);
}

void ArchetypeMigrator::MoveEntity(EntityId  entityId,
                                    Archetype& source,
                                    Archetype& target)
{
    // Remove entity from old archetype's entity list.
    source.RemoveEntity(entityId);

    // Add entity to new archetype's entity list.
    target.AddEntity(entityId);
}

} // namespace SagaEngine::ECS
