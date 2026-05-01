/// @file SimCell.cpp
/// @brief Implementation of the dynamic simulation cell.

#include "SagaEngine/World/SimCell.h"
#include "SagaEngine/Core/Time/Time.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::World {

static constexpr const char* kTag = "SimCell";

void SimCell::AddEntity(EntityId id) noexcept
{
    if (!m_entities.insert(id).second)
        return; // already present

    m_metrics.entityCount = static_cast<uint32_t>(m_entities.size());
    m_metrics.lastActivityTick = static_cast<uint64_t>(Core::Time::GetTime() * 60.0); // ~60hz tick count

    SimCellEvent evt;
    evt.type      = SimCellEventType::EntitySpawned;
    evt.entityId  = id;
    evt.timestamp = static_cast<uint64_t>(Core::Time::GetTime() * 1'000'000.0);
    m_events.push_back(evt);
}

void SimCell::RemoveEntity(EntityId id) noexcept
{
    if (!m_entities.erase(id))
        return;

    m_metrics.entityCount = static_cast<uint32_t>(m_entities.size());
    m_metrics.lastActivityTick = Core::Time::GetTime() * 1000.0;

    SimCellEvent evt;
    evt.type      = SimCellEventType::EntityDespawned;
    evt.entityId  = id;
    evt.timestamp = static_cast<uint64_t>(Core::Time::GetTime() * 1'000'000.0);
    m_events.push_back(evt);
}

bool SimCell::HasEntity(EntityId id) const noexcept
{
    return m_entities.contains(id);
}

void SimCell::UpdateMetrics() noexcept
{
    // Recount entity types.  In production, the ECS would tag entities
    // as Player/NPC, and we'd query that here.  For now we use a
    // placeholder: first N entities are players.
    m_metrics.entityCount = static_cast<uint32_t>(m_entities.size());

    // Update the low-state timer for merge decisions.
    if (m_metrics.entityCount < m_thresholds.minEntities)
    {
        if (m_lowStateStartTick == 0)
            m_lowStateStartTick = static_cast<uint64_t>(Core::Time::GetTime() * 60.0); // ~60hz tick count
    }
    else
    {
        m_lowStateStartTick = 0;
    }
}

// ShouldSplit / ShouldMerge moved to SimCell_SpatialLogic.cpp to keep the
// spatial-partitioning policy in one place. Defining them here too caused a
// duplicate-symbol link warning (LNK4006) when both translation units were
// compiled into SagaEngine.lib.

std::pair<SimCellId, SimCellId>
SimCell::ExecuteSplit(PositionQueryFn positionQuery) const noexcept
{
    if (!positionQuery)
    {
        LOG_ERROR(kTag, "Cell %d,%d: ExecuteSplit called with null positionQuery",
                  m_id.worldX, m_id.worldZ);
        return {m_id, m_id};  // Invalid: caller should handle.
    }

    // Determine split axis: choose the longer axis of the footprint.
    const bool splitOnX = (m_footprint.halfExtents.x >= m_footprint.halfExtents.z);
    const float splitThreshold = splitOnX
                                     ? m_footprint.centre.x
                                     : m_footprint.centre.z;

    // Compute child cell coordinates.
    // Child A: lower half along split axis; Child B: upper half.
    const int16_t childAX = splitOnX
                                ? static_cast<int16_t>(m_id.worldX * 2)
                                : static_cast<int16_t>(m_id.worldX);
    const int16_t childAZ = splitOnX
                                ? static_cast<int16_t>(m_id.worldZ)
                                : static_cast<int16_t>(m_id.worldZ * 2);
    const int16_t childBX = splitOnX
                                ? static_cast<int16_t>(m_id.worldX * 2 + 1)
                                : static_cast<int16_t>(m_id.worldX);
    const int16_t childBZ = splitOnX
                                ? static_cast<int16_t>(m_id.worldZ)
                                : static_cast<int16_t>(m_id.worldZ * 2 + 1);

    const uint16_t newGen = static_cast<uint16_t>(m_id.gen + 1);
    SimCellId idA = {childAX, childAZ, newGen};
    SimCellId idB = {childBX, childBZ, newGen};

    // Partition entities by position using the callback.
    std::vector<EntityId> entitiesA, entitiesB;
    entitiesA.reserve(m_entities.size() / 2 + 1);
    entitiesB.reserve(m_entities.size() - entitiesA.capacity() + 1);

    uint32_t unpositionedCount = 0;

    for (EntityId eid : m_entities)
    {
        Vec3 position;
        if (positionQuery(eid, position))
        {
            // Position-based assignment.
            const float coord = splitOnX ? position.x : position.z;
            if (coord < splitThreshold)
                entitiesA.push_back(eid);
            else
                entitiesB.push_back(eid);
        }
        else
        {
            // Fallback: entity has no Transform; assign by ID hash.
            // This ensures deterministic partitioning even for unpositioned entities.
            if ((static_cast<uint32_t>(eid) & 1u) == 0u)
                entitiesA.push_back(eid);
            else
                entitiesB.push_back(eid);
            unpositionedCount++;
        }
    }

    if (unpositionedCount > 0)
    {
        LOG_WARN(kTag, "Cell %d,%d: %u entities lack Transform; using ID hash fallback",
                 m_id.worldX, m_id.worldZ, unpositionedCount);
    }

    // Log the split event.
    SimCellEvent evt;
    evt.type      = SimCellEventType::SplitInitiated;
    evt.timestamp = static_cast<uint64_t>(Core::Time::GetTime() * 1'000'000.0);
    evt.data      = static_cast<uint32_t>(m_entities.size());
    m_events.push_back(evt);

    LOG_INFO(kTag, "Cell %d,%d splitting on %s axis: %u entities → A(%d,%d), %u entities → B(%d,%d)",
             m_id.worldX, m_id.worldZ,
             splitOnX ? "X" : "Z",
             static_cast<uint32_t>(entitiesA.size()), idA.worldX, idA.worldZ,
             static_cast<uint32_t>(entitiesB.size()), idB.worldX, idB.worldZ);

    // Caller (WorldNode) is responsible for creating child cells and moving entities.
    // The partition lists are returned via the event system or direct callback.
    // For the vertical slice, the WorldNode will query this cell's partition lists.
    // Production: this would directly move entities via WorldNode reference.

    return {idA, idB};
}

void SimCell::ExecuteMergeInto(SimCell& target) noexcept
{
    // Transfer all entities from this cell to the target.
    for (EntityId eid : m_entities)
        target.AddEntity(eid);

    // Log the merge event.
    SimCellEvent evt;
    evt.type      = SimCellEventType::MergeCompleted;
    evt.timestamp = static_cast<uint64_t>(Core::Time::GetTime() * 1'000'000.0);
    evt.data      = static_cast<uint32_t>(m_entities.size());
    m_events.push_back(evt);
    target.AppendEvent(evt);

    // Clear this cell.
    m_entities.clear();
    m_metrics.entityCount = 0;
    m_metrics.playerCount = 0;
    m_metrics.npcCount = 0;
    m_lowStateStartTick = 0;
    SetState(SimCellState::Dormant);
}

void SimCell::AppendEvent(SimCellEvent evt) noexcept
{
    m_events.push_back(std::move(evt));
}

std::vector<SimCellEvent> SimCell::DrainEvents() noexcept
{
    std::vector<SimCellEvent> out = std::move(m_events);
    m_events.clear();
    return out;
}

void SimCell::SetState(SimCellState newState) noexcept
{
    if (m_state == newState)
        return;

    SimCellEvent evt;
    evt.type      = SimCellEventType::Created; // generic structural event
    evt.timestamp = static_cast<uint64_t>(Core::Time::GetTime() * 1'000'000.0);
    evt.data      = static_cast<uint32_t>(newState);
    m_events.push_back(evt);

    m_state = newState;
}

} // namespace SagaEngine::World
