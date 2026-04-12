/// @file SimCell.cpp
/// @brief Implementation of the dynamic simulation cell.

#include "SagaEngine/World/SimCell.h"
#include "SagaEngine/Core/Time/Time.h"

namespace SagaEngine::World {

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

bool SimCell::ShouldSplit() const noexcept
{
    return m_metrics.entityCount > m_thresholds.maxEntities ||
           m_metrics.playerCount > m_thresholds.maxPlayers;
}

bool SimCell::ShouldMerge() const noexcept
{
    if (m_metrics.entityCount >= m_thresholds.minEntities)
        return false;

    if (m_lowStateStartTick == 0)
        return false;

    const uint64_t currentTick = static_cast<uint64_t>(Core::Time::GetTime() * 60.0);
    return (currentTick - m_lowStateStartTick) >= m_thresholds.mergeGraceTicks;
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
