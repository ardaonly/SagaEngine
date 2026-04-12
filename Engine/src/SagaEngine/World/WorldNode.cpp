/// @file WorldNode.cpp
/// @brief World Kernel node implementation.

#include "SagaEngine/World/WorldNode.h"
#include "SagaEngine/Core/Time/Time.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Math/Vec3.h"

#include <chrono>

namespace SagaEngine::World {

using namespace std::chrono;

static constexpr const char* kTag = "WorldNode";

bool WorldNode::Init(const WorldNodeConfig& config) noexcept
{
    m_config = config;
    m_worldTick = 0;
    m_stats = WorldNodeStats{};

    // Register default domains.
    RegisterDomain(SimDomainType::Combat,   "Combat",   60);
    RegisterDomain(SimDomainType::Movement, "Movement", 20);
    RegisterDomain(SimDomainType::Economy,  "Economy",   1);
    RegisterDomain(SimDomainType::Politics, "Politics",  0);  // Event-driven
    RegisterDomain(SimDomainType::Ecology,  "Ecology",   0);  // Event-driven
    RegisterDomain(SimDomainType::Narrative,"Narrative",  0);  // Event-driven

    LOG_INFO(kTag, "WorldNode '%s' initialized. Tick rate: %.0f Hz, max clients: %u",
             m_config.nodeId.c_str(), m_config.worldTickHz, m_config.maxClients);
    return true;
}

void WorldNode::Shutdown() noexcept
{
    LOG_INFO(kTag, "WorldNode shutting down. Ticks completed: %llu",
             static_cast<unsigned long long>(m_stats.ticksCompleted));

    m_clients.clear();
    m_cells.clear();
    m_entityCells.clear();
}

void WorldNode::Tick() noexcept
{
    const auto tickStart = steady_clock::now();
    m_worldTick++;

    // Phase 1: Drain client input.
    DrainInput();

    // Phase 2: Step the simulation (ECS systems run here).
    StepSimulation();

    // Phase 3: Fire domains at their scheduled rates.
    FireDomains();

    // Phase 4: Update the relevance graph.
    UpdateRelevance();

    // Phase 5: Replicate state to clients.
    ReplicateToClients();

    // Phase 6: Flush cell events to the event stream.
    FlushEvents();

    const auto tickEnd = steady_clock::now();
    m_stats.tickDurationMs = static_cast<float>(
        duration_cast<microseconds>(tickEnd - tickStart).count()) / 1000.0f;
    m_stats.lastTickWorldTick = m_worldTick;
    m_stats.ticksCompleted++;
}

SimCell* WorldNode::GetOrCreateCell(SimCellId id) noexcept
{
    const auto key = PackCellCoord(id);
    auto it = m_cells.find(key);
    if (it != m_cells.end())
        return &it->second;

    // Create new cell.
    SimCellFootprint fp;
    fp.centre = Vec3(
        (static_cast<float>(id.worldX) + 0.5f) * m_config.cellSize,
        0.0f,
        (static_cast<float>(id.worldZ) + 0.5f) * m_config.cellSize);
    fp.halfExtents = Vec3(m_config.cellSize * 0.5f, 256.0f, m_config.cellSize * 0.5f);

    auto [newIt, inserted] = m_cells.emplace(key, SimCell(id, std::move(fp)));
    if (inserted)
    {
        m_stats.cellsActive = m_cells.size();
        newIt->second.SetState(SimCellState::Loading);

        // Append cell creation event.
        WorldEvent evt;
        evt.type      = WorldEventType::CellCreated;
        evt.cellId    = id;
        evt.worldTick = m_worldTick;
        evt.timestampUs = static_cast<uint64_t>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
        m_eventStream.Append(std::move(evt));
    }

    return &newIt->second;
}

SimCell* WorldNode::GetCell(SimCellId id) noexcept
{
    const auto key = PackCellCoord(id);
    auto it = m_cells.find(key);
    return it != m_cells.end() ? &it->second : nullptr;
}

const SimCell* WorldNode::GetCell(SimCellId id) const noexcept
{
    const auto key = PackCellCoord(id);
    auto it = m_cells.find(key);
    return it != m_cells.end() ? &it->second : nullptr;
}

void WorldNode::RemoveCell(SimCellId id) noexcept
{
    const auto key = PackCellCoord(id);

    // Remove all entities in this cell.
    auto it = m_cells.find(key);
    if (it != m_cells.end())
    {
        for (EntityId eid : it->second.Entities())
            m_entityCells.erase(eid);

        m_cells.erase(it);
        m_stats.cellsActive = m_cells.size();

        m_scheduler.UnregisterCellAll(id);
    }
}

void WorldNode::SpawnEntity(EntityId id, SimCellId cellId) noexcept
{
    SimCell* cell = GetOrCreateCell(cellId);
    if (!cell)
        return;

    cell->AddEntity(id);
    m_entityCells[id] = PackCellCoord(cellId);
    m_stats.entitiesTotal = m_entityCells.size();

    WorldEvent evt;
    evt.type      = WorldEventType::EntitySpawned;
    evt.cellId    = cellId;
    evt.entityId  = id;
    evt.worldTick = m_worldTick;
    evt.timestampUs = static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    m_eventStream.Append(std::move(evt));
}

void WorldNode::DespawnEntity(EntityId id) noexcept
{
    auto it = m_entityCells.find(id);
    if (it == m_entityCells.end())
        return;

    const SimCellId cellId = UnpackCellCoord(it->second);
    SimCell* cell = GetCell(cellId);
    if (cell)
        cell->RemoveEntity(id);

    m_entityCells.erase(it);
    m_stats.entitiesTotal = m_entityCells.size();

    WorldEvent evt;
    evt.type      = WorldEventType::EntityDespawned;
    evt.cellId    = cellId;
    evt.entityId  = id;
    evt.worldTick = m_worldTick;
    evt.timestampUs = static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    m_eventStream.Append(std::move(evt));
}

void WorldNode::MoveEntity(EntityId id, SimCellId /*fromCell*/, SimCellId toCell) noexcept
{
    auto it = m_entityCells.find(id);
    if (it == m_entityCells.end())
        return;

    const uint32_t oldKey = it->second;
    it->second = PackCellCoord(toCell);

    SimCell* oldCell = GetCell(UnpackCellCoord(oldKey));
    if (oldCell)
        oldCell->RemoveEntity(id);

    SimCell* newCell = GetOrCreateCell(toCell);
    if (newCell)
        newCell->AddEntity(id);

    WorldEvent evt;
    evt.type      = WorldEventType::EntityMoved;
    evt.cellId    = toCell;
    evt.entityId  = id;
    evt.worldTick = m_worldTick;
    evt.timestampUs = static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    m_eventStream.Append(std::move(evt));
}

SimCell* WorldNode::GetEntityCell(EntityId id) noexcept
{
    auto it = m_entityCells.find(id);
    if (it == m_entityCells.end())
        return nullptr;
    return GetCell(UnpackCellCoord(it->second));
}

uint64_t WorldNode::AddClientSession(EntityId controlledEntity) noexcept
{
    const uint64_t sid = m_nextSessionId++;
    ClientSession session;
    session.sessionId        = sid;
    session.controlledEntity = controlledEntity;
    session.wantsFullSnapshot = true;
    m_clients[sid] = std::move(session);
    m_stats.clientsConnected = m_clients.size();

    WorldEvent evt;
    evt.type      = WorldEventType::PlayerConnected;
    evt.entityId  = controlledEntity;
    evt.worldTick = m_worldTick;
    evt.timestampUs = static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    m_eventStream.Append(std::move(evt));

    return sid;
}

void WorldNode::RemoveClientSession(uint64_t sessionId) noexcept
{
    auto it = m_clients.find(sessionId);
    if (it != m_clients.end())
    {
        WorldEvent evt;
        evt.type      = WorldEventType::PlayerDisconnected;
        evt.entityId  = it->second.controlledEntity;
        evt.worldTick = m_worldTick;
        evt.timestampUs = static_cast<uint64_t>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
        m_eventStream.Append(std::move(evt));

        m_clients.erase(it);
        m_stats.clientsConnected = m_clients.size();
    }
}

ClientSession* WorldNode::GetClientSession(uint64_t id) noexcept
{
    auto it = m_clients.find(id);
    return it != m_clients.end() ? &it->second : nullptr;
}

bool WorldNode::ReplicateToClient(uint64_t sessionId) noexcept
{
    auto* session = GetClientSession(sessionId);
    if (!session)
        return false;

    // For now, send new events since the client's last seen sequence.
    auto newEvents = m_eventStream.GetEventsAfter(session->lastSeenEventSeq);
    if (newEvents.empty())
        return false;

    // TODO: Serialize events into snapshot/delta format for the client.
    // For the first vertical slice, we just track that replication happened.
    session->lastSeenEventSeq = m_eventStream.LastSequence();

    if (session->wantsFullSnapshot)
    {
        session->wantsFullSnapshot = false;
        m_stats.snapshotsSent++;
    }
    else
    {
        m_stats.deltasSent++;
    }

    return true;
}

void WorldNode::AppendEvent(WorldEvent evt) noexcept
{
    evt.worldTick = m_worldTick;
    m_eventStream.Append(std::move(evt));
    m_stats.eventsAppended = m_eventStream.EventCount();
}

void WorldNode::RegisterDomain(SimDomainType type, const char* name,
                                  uint32_t ticksPerSecond) noexcept
{
    m_scheduler.RegisterDomain(type, name, ticksPerSecond);
}

// ─── Internal tick phases ─────────────────────────────────────────────────────

void WorldNode::DrainInput() noexcept
{
    // TODO: Drain input from client sessions (input commands, etc.).
    // For the first vertical slice, this is a no-op.
}

void WorldNode::StepSimulation() noexcept
{
    // TODO: Run ECS systems on the WorldState.
    // The actual simulation logic (movement, combat, etc.) runs here.
    // For the first vertical slice, this is a no-op.
}

void WorldNode::FireDomains() noexcept
{
    std::vector<SimDomainType> firing;
    m_scheduler.Tick(m_worldTick, firing);

    if (firing.empty() || !m_domainDispatcher)
        return;

    for (SimDomainType domain : firing)
    {
        const auto& cells = m_scheduler.GetDomainCells(domain);
        for (const auto& cellId : cells)
        {
            SimCell* cell = GetCell(cellId);
            if (!cell || cell->State() != SimCellState::Active)
                continue;

            const auto* desc = m_scheduler.GetDomain(domain);
            float subTick = 0.0f;
            if (desc)
            {
                const auto& stats = m_scheduler.GetDomainStats(domain);
                subTick = stats.subTickAccumulator;
            }

            m_domainDispatcher(domain, *cell, subTick);
        }
    }
}

void WorldNode::UpdateRelevance() noexcept
{
    // Rebuild the relevance graph every tick (for now).
    // In production, this would be incremental or at a lower frequency.

    // Collect all entities.
    std::vector<EntityId> entities;
    entities.reserve(m_entityCells.size());
    for (const auto& [id, _] : m_entityCells)
        entities.push_back(id);

    // Set entities on the graph.
    // Note: the graph doesn't have a SetEntities method; we'd need to add
    // one or use the entities internally.  For now, skip.
    m_stats.relevanceQueries += 1; // placeholder
}

void WorldNode::ReplicateToClients() noexcept
{
    for (const auto& [sid, _] : m_clients)
        ReplicateToClient(sid);
}

void WorldNode::FlushEvents() noexcept
{
    // Drain cell events to the global event stream.
    for (auto& [key, cell] : m_cells)
    {
        auto events = cell.DrainEvents();
        const SimCellId cellId = UnpackCellCoord(key);
        for (auto& evt : events)
        {
            WorldEvent we;
            we.type      = static_cast<WorldEventType>(static_cast<uint16_t>(evt.type));
            we.cellId    = cellId;
            we.entityId  = evt.entityId;
            we.worldTick = m_worldTick;
            we.timestampUs = evt.timestamp;
            we.data      = evt.data;
            m_eventStream.Append(std::move(we));
        }
    }
    m_stats.eventsAppended = m_eventStream.EventCount();
}

} // namespace SagaEngine::World
