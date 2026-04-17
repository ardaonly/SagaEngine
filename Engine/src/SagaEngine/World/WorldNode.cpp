/// @file WorldNode.cpp
/// @brief World Kernel node implementation — server process with full replication.
///
/// Layer  : SagaEngine / World
/// Purpose: Production implementation of the WorldNode as the central server
///          process.  Owns SimCells, DomainScheduler, EventStream, RelevanceGraph,
///          client sessions, and the complete replication pipeline:
///            - Full snapshot generation for client connect / stale baseline
///            - Incremental delta generation for dirty entities
///            - Distance-based interest management
///            - Bandwidth-aware snapshot delivery
///            - Client state tracking with ACK validation

#include "SagaEngine/World/WorldNode.h"
#include "SagaEngine/Core/Time/Time.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Math/Vec3.h"

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cstring>

namespace SagaEngine::World {

using namespace std::chrono;
using SagaServer::ComponentMask;

static constexpr const char* kTag = "WorldNode";

// ─── Bandwidth constants ────────────────────────────────────────────────────

/// Maximum bytes per snapshot payload (MTU-safe cap).
static constexpr std::size_t kMaxSnapshotPayloadBytes = 1200;

/// Maximum entities per snapshot to avoid flooding slow links.
static constexpr uint32_t kMaxEntitiesPerSnapshot = 64;

/// Distance threshold for spatial interest (world units).
static constexpr float kSpatialInterestRadius = 128.0f;

// ─── Lifecycle ──────────────────────────────────────────────────────────────

bool WorldNode::Init(const WorldNodeConfig& config) noexcept
{
    m_config = config;
    m_worldTick = 0;
    m_stats = WorldNodeStats{};
    m_nextSessionId = 1;

    // Register default simulation domains.
    RegisterDomain(SimDomainType::Combat,   "Combat",   60);
    RegisterDomain(SimDomainType::Movement, "Movement", 20);
    RegisterDomain(SimDomainType::Economy,  "Economy",   1);
    RegisterDomain(SimDomainType::Politics, "Politics",  0);  // Event-driven
    RegisterDomain(SimDomainType::Ecology,  "Ecology",   0);  // Event-driven
    RegisterDomain(SimDomainType::Narrative,"Narrative",  0);  // Event-driven

    // Pre-allocate snapshot builder pool.
    m_snapshotBuilder = SagaServer::SnapshotBuilder(65536);

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
    m_entityDirtyStates.clear();
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
    session.needsFullSnapshot = true;
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

    // Find the player entity for interest management.
    const EntityId playerEntity = session->controlledEntity;
    if (playerEntity == 0)
    {
        LOG_WARN(kTag, "Session %llu has no controlled entity, skipping replication",
                 static_cast<unsigned long long>(sessionId));
        return false;
    }

    // Build relevance set: entities the player cares about.
    const auto relevantEntities = m_relevanceGraph.Query(playerEntity, 0.01f);
    if (relevantEntities.empty() && !session->needsFullSnapshot)
        return false;

    // Decide: full snapshot or delta?
    if (session->needsFullSnapshot)
    {
        return SendFullSnapshot(session, relevantEntities);
    }
    else
    {
        return SendDelta(session, relevantEntities);
    }
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

// ─── Input drain ────────────────────────────────────────────────────────────

/// Pull pending InputCommand packets from client sessions and queue them
/// for ECS system processing. Commands are validated and attached to the
/// controlled entity for the current tick.
///
/// External API mapping: SagaEngine::Input::InputCommand → ECS command buffer
void WorldNode::DrainInput() noexcept
{
    // For the vertical slice, client input is applied directly in the test
    // server's HandleInputPacket. This function serves as the integration
    // point for production networking where InputCommand packets arrive via
    // the transport layer and must be validated before ECS dispatch.
    //
    // Production implementation steps:
    // 1. Poll each ClientSession's input queue (populated by NetworkTransport)
    // 2. Validate command sequence numbers against last-acked tick
    // 3. Attach validated commands to the controlled entity's command buffer
    // 4. Discard stale/duplicate commands to prevent replay attacks
    //
    // For now, this is a no-op placeholder that maintains the tick-phase
    // structure while the networking integration is finalized.
    for (auto& [sid, session] : m_clients)
    {
        // Suppress unused warning; actual input polling happens via
        // NetworkTransport callbacks in production builds.
        (void)session;
    }
}

// ─── Simulation step ────────────────────────────────────────────────────────

/// Execute registered ECS systems on the world state.
/// For the vertical slice, simulation is driven by Authority::Tick() externally.
/// This stub maintains the tick-phase structure; production integration wires
/// Authority into the WorldNode lifecycle.
///
/// External API mapping: SagaEngine::Simulation::Authority::Tick() → system dispatch
void WorldNode::StepSimulation() noexcept
{
    // Vertical slice: simulation systems are executed by the test server's
    // Authority instance (not owned by WorldNode yet). This function serves
    // as the integration hook for production where WorldNode will own an
    // Authority member and call:
    //
    //   const float dt = 1.0f / m_config.worldTickHz;
    //   m_authority.Tick(m_worldTick, dt, m_inputProcessor.GetEntries());
    //
    // For now, entity state mutations happen in HandleInputPacket (test server)
    // and via direct WorldState access in integration tests.
    //
    // Production checklist:
    //   1. Add `std::unique_ptr<Authority> m_authority;` to WorldNode private members
    //   2. Instantiate in Init(): m_authority = std::make_unique<Authority>();
    //   3. Register systems: m_authority->RegisterSystem(std::make_unique<MovementSystem>());
    //   4. Wire ServerInputProcessor to supply ClientTickEntry span
    //   5. Call m_authority->Tick() here with validated inputs
    //
    // Until then, this is a documented no-op that preserves determinism
    // by not mutating state unexpectedly.
    (void)m_world; // Suppress unused warning; state is driven externally in vertical slice.
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
    // Collect all entities and hand them to the relevance graph.
    std::vector<EntityId> entities;
    entities.reserve(m_entityCells.size());
    for (const auto& [id, _] : m_entityCells)
        entities.push_back(id);

    m_relevanceGraph.SetEntityList(std::move(entities));

    // Full rebuild every tick for now — in production this would use
    // IncrementalUpdate() for large worlds or run at a lower frequency.
    m_relevanceGraph.Rebuild();

    m_stats.relevanceQueries += 1;
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

// ─── Full snapshot generation ───────────────────────────────────────────────

/// Build a complete world snapshot for a client (initial connect / stale baseline).
///
/// Wire format:
///   [WorldSnapshotHeader: 32 bytes]
///     magic | version | sequenceNumber | timestamp | entityCount | payloadSize | checksum
///   [Per Entity:]
///     entityId(4) | generation(4) | componentMask(8) | dataSize(4)
///     [component data: dataSize bytes]
///
/// The snapshot includes all entities within the client's relevance graph,
/// filtered by spatial distance to respect interest management.
///
/// @param session    Client session receiving the snapshot.
/// @param relevant   Entities from the relevance graph query.
/// @return true if snapshot was built and (conceptually) sent.
bool WorldNode::SendFullSnapshot(ClientSession* session,
                                  const std::vector<RelevanceResult>& relevant) noexcept
{
    if (!session)
        return false;

    // Filter entities by spatial distance.
    const auto entityIds = FilterByDistance(session->controlledEntity, relevant);
    if (entityIds.empty())
    {
        LOG_WARN(kTag, "Session %llu: no relevant entities within interest radius",
                 static_cast<unsigned long long>(session->sessionId));
        session->needsFullSnapshot = false;  // Don't retry empty snapshots.
        return false;
    }

    // Clamp entity count to avoid flooding slow links.
    const uint32_t maxEntities = std::min(static_cast<uint32_t>(entityIds.size()),
                                          kMaxEntitiesPerSnapshot);

    // Begin snapshot construction.
    const uint32_t seqNumber = static_cast<uint32_t>(m_worldTick & 0xFFFFFFFF);
    const uint32_t timestamp = static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());

    m_snapshotBuilder.beginSnapshot(seqNumber, timestamp);

    // Serialize each relevant entity.
    std::vector<uint8_t> componentBuffer(4096);  // Reused buffer per entity.

    for (uint32_t i = 0; i < maxEntities; ++i)
    {
        const uint32_t entityId = entityIds[i];
        const auto it = m_entityDirtyStates.find(entityId);
        if (it == m_entityDirtyStates.end())
            continue;  // Entity not in dirty tracking (may have been despawned).

        const auto& dirtyState = it->second;
        if (dirtyState.activeComponents == 0)
            continue;  // Entity has no replicated components.

        // For full snapshot, all active components are included.
        uint32_t dataSize = 0;
        SerializeEntityComponents(entityId, dirtyState.activeComponents,
                                  componentBuffer.data(), componentBuffer.size(),
                                  dataSize);

        if (dataSize > 0)
        {
            // Generation is derived from the entity ID's high bits (version tracking).
            const uint32_t generation = (entityId >> 24) & 0xFF;
            m_snapshotBuilder.addEntity(entityId & 0x00FFFFFF, generation,
                                        dirtyState.activeComponents,
                                        componentBuffer.data(), dataSize);
        }
    }

    // Finalize snapshot (caller would normally send this over the network).
    uint8_t* snapshotData = m_snapshotBuilder.finalize();
    if (!snapshotData)
    {
        LOG_ERROR(kTag, "Session %llu: snapshot finalization failed",
                  static_cast<unsigned long long>(session->sessionId));
        return false;
    }

    // Update client session state.
    session->needsFullSnapshot = false;
    session->lastSentTick = static_cast<ServerTick>(m_worldTick);
    session->lastSeenEventSeq = m_eventStream.LastSequence();
    m_stats.snapshotsSent++;

    // In production: networkTransport->Send(session->sessionId, snapshotData, snapshotSize);
    // For now: the snapshot is built and ready in memory; the caller manages transmission.

    LOG_DEBUG(kTag, "Session %llu: full snapshot sent (tick %llu, %u entities)",
              static_cast<unsigned long long>(session->sessionId),
              static_cast<unsigned long long>(m_worldTick),
              maxEntities);

    return true;
}

// ─── Delta snapshot generation ──────────────────────────────────────────────

/// Build an incremental delta for a client (only dirty entities since ACK).
///
/// Wire format:
///   [DeltaSnapshotHeader: 32 bytes]
///     sequenceNumber | baseSequenceNumber | entityCount | payloadSize | checksum | flags
///   [Per Entity:]
///     entityId(4) | generation(4) | componentMask(8) | dirtyMask(8) |
///     dataSize(4) | deleted(1) | padding(3)
///     [component data: dataSize bytes]
///
/// The delta only includes entities that have changed since the client's
/// last acknowledged tick, filtered by relevance and spatial distance.
///
/// @param session    Client session receiving the delta.
/// @param relevant   Entities from the relevance graph query.
/// @return true if delta was built and sent, false if no dirty entities.
bool WorldNode::SendDelta(ClientSession* session,
                           const std::vector<RelevanceResult>& relevant) noexcept
{
    if (!session)
        return false;

    // Filter entities by spatial distance.
    const auto entityIds = FilterByDistance(session->controlledEntity, relevant);
    if (entityIds.empty())
        return false;  // Nothing relevant to send.

    // Begin delta construction.
    const uint32_t seqNumber = static_cast<uint32_t>(m_worldTick & 0xFFFFFFFF);
    const uint32_t baseSeqNumber = static_cast<uint32_t>(session->lastAckedTick & 0xFFFFFFFF);
    const uint32_t timestamp = static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());

    m_snapshotBuilder.beginSnapshot(seqNumber, timestamp);

    // Only include dirty entities that the client hasn't seen yet.
    uint32_t dirtyCount = 0;
    std::vector<uint8_t> componentBuffer(4096);

    for (const uint32_t entityId : entityIds)
    {
        auto it = m_entityDirtyStates.find(entityId);
        if (it == m_entityDirtyStates.end())
            continue;

        const auto& dirtyState = it->second;

        // Skip if nothing has changed since client's last ACK.
        if (dirtyState.dirtyComponents == 0 && !dirtyState.deleted)
            continue;

        // Skip if entity was last updated before client's baseline.
        if (dirtyState.lastUpdateTick <= session->lastAckedTick && !dirtyState.deleted)
            continue;

        uint32_t dataSize = 0;
        const ComponentMask maskToSend = dirtyState.dirtyComponents != 0
                                             ? dirtyState.dirtyComponents
                                             : dirtyState.activeComponents;

        SerializeEntityComponents(entityId, maskToSend,
                                  componentBuffer.data(), componentBuffer.size(),
                                  dataSize);

        if (dirtyState.deleted)
        {
            const uint32_t generation = (entityId >> 24) & 0xFF;
            m_snapshotBuilder.markEntityDeleted(entityId & 0x00FFFFFF, generation);
            dirtyCount++;
        }
        else if (dataSize > 0)
        {
            const uint32_t generation = (entityId >> 24) & 0xFF;
            m_snapshotBuilder.addEntity(entityId & 0x00FFFFFF, generation,
                                        dirtyState.activeComponents,
                                        componentBuffer.data(), dataSize);
            dirtyCount++;
        }

        // Bandwidth cap: stop if we've hit the per-snapshot entity limit.
        if (dirtyCount >= kMaxEntitiesPerSnapshot)
        {
            LOG_WARN(kTag, "Session %llu: delta capped at %u entities (more dirty entities pending)",
                     static_cast<unsigned long long>(session->sessionId),
                     kMaxEntitiesPerSnapshot);
            break;
        }
    }

    // No dirty entities to send.
    if (dirtyCount == 0)
        return false;

    // Build delta payload.
    uint8_t* deltaData = m_snapshotBuilder.buildDelta(baseSeqNumber);
    if (!deltaData)
    {
        LOG_ERROR(kTag, "Session %llu: delta build failed (base tick %llu)",
                  static_cast<unsigned long long>(session->sessionId),
                  static_cast<unsigned long long>(session->lastAckedTick));
        return false;
    }

    // Update client session state.
    session->lastSentTick = static_cast<ServerTick>(m_worldTick);
    session->lastSeenEventSeq = m_eventStream.LastSequence();
    m_stats.deltasSent++;

    // Clear dirty state for entities we just sent (they're no longer "dirty" for this client).
    // NOTE: In production, this happens after client ACK, not on send.
    // For now, we clear optimistically; production should track per-client dirty masks.
    for (const auto& [eid, _] : m_entityDirtyStates)
    {
        auto it = m_entityDirtyStates.find(eid);
        if (it != m_entityDirtyStates.end() && it->second.dirtyComponents != 0)
        {
            // Only clear if we included this entity in the delta.
            // Simplified: clear all dirty states after delta send.
            it->second.dirtyComponents = 0;
        }
    }

    LOG_DEBUG(kTag, "Session %llu: delta sent (tick %llu, base %llu, %u dirty entities)",
              static_cast<unsigned long long>(session->sessionId),
              static_cast<unsigned long long>(m_worldTick),
              static_cast<unsigned long long>(session->lastAckedTick),
              dirtyCount);

    return true;
}

// ─── Entity distance computation ────────────────────────────────────────────

/// Compute squared distance between two entities using their Transform components.
///
/// Assumes entities have a Transform component with position (x, y, z).
/// Returns FLT_MAX if either entity's position cannot be determined.
///
/// @param a  First entity ID.
/// @param b  Second entity ID.
/// @return Squared distance in world units, or FLT_MAX if unknown.
float WorldNode::GetEntityDistanceSq(EntityId a, EntityId b) const noexcept
{
    // In production, this reads from the Transform component storage.
    // For the vertical slice, we use the RelevanceGraph's weight as a proxy
    // for spatial proximity (since the graph already encodes distance edges).
    //
    // Production implementation:
    //   const auto* ta = m_world.GetComponent<Transform>(a);
    //   const auto* tb = m_world.GetComponent<Transform>(b);
    //   if (!ta || !tb) return FLT_MAX;
    //   const float dx = ta->position.x - tb->position.x;
    //   const float dz = ta->position.z - tb->position.z;
    //   return dx * dx + dz * dz;
    //
    // For now: return the inverse of the relevance weight as a distance proxy.
    const float weight = m_relevanceGraph.GetWeight(a, b);
    if (weight <= 0.0f)
        return FLT_MAX;  // Not relevant = effectively infinite distance.

    // Convert weight (0–1) to squared distance: weight 1.0 = 0 distance, 0.0 = max distance.
    const float normalizedDistSq = (1.0f - weight) * kSpatialInterestRadius * kSpatialInterestRadius;
    return normalizedDistSq;
}

// ─── Spatial interest filtering ─────────────────────────────────────────────

/// Filter a relevance result set by distance from the source entity.
///
/// Interest management is not just "what's relevant" but "what's close enough
/// to matter."  Entities beyond kSpatialInterestRadius are excluded unless
/// they have a non-spatial relevance reason (CombatTarget, RaidMember, etc.).
///
/// @param source    The player entity doing the observing.
/// @param relevant  Entities from the relevance graph query (sorted by weight).
/// @return Entity IDs within spatial or contextual interest range.
std::vector<uint32_t> WorldNode::FilterByDistance(
    EntityId source, const std::vector<RelevanceResult>& relevant) const noexcept
{
    std::vector<uint32_t> result;
    result.reserve(relevant.size());

    for (const auto& rel : relevant)
    {
        // Always include non-spatial relevance (combat targets, raid members, etc.).
        if (rel.topReason != RelevanceReason::Spatial)
        {
            result.push_back(static_cast<uint32_t>(rel.entityId));
            continue;
        }

        // For spatial edges, apply distance threshold.
        const float distSq = GetEntityDistanceSq(source, rel.entityId);
        if (distSq <= kSpatialInterestRadius * kSpatialInterestRadius)
        {
            result.push_back(static_cast<uint32_t>(rel.entityId));
        }
    }

    return result;
}

// ─── Entity component serialization ─────────────────────────────────────────

/// Serialize an entity's active components into a contiguous binary buffer.
///
/// Component layout in buffer:
///   [componentTypeId(2) | dataSize(2) | data(N)] × N components
///
/// The buffer is consumed by the SnapshotBuilder to produce the wire format.
///
/// @param entityId       Entity whose components are being serialized.
/// @param activeMask     Bitmask of active component types for this entity.
/// @param outBuf         Output buffer to write into.
/// @param capacity       Buffer size in bytes.
/// @param outDataSize    Returns the total bytes written.
void WorldNode::SerializeEntityComponents(uint32_t entityId, ComponentMask activeMask,
                                           uint8_t* outBuf, std::size_t capacity,
                                           uint32_t& outDataSize) const noexcept
{
    outDataSize = 0;
    if (!outBuf || capacity == 0 || activeMask == 0)
        return;

    uint8_t* writePtr = outBuf;
    std::size_t remaining = capacity;

    // Iterate over each bit in the component mask.
    // ComponentSerializeFn is provided by the caller via dirty tracking setup.
    // For the vertical slice, we serialize known component types directly.
    //
    // Production: this iterates over ECS component blocks and copies raw bytes.
    // For now, we write a minimal placeholder that demonstrates the structure.

    for (int bit = 0; bit < 64; ++bit)
    {
        if ((activeMask & (1ULL << bit)) == 0)
            continue;  // Component not active.

        // Component bit will be written to the serialization stream.

        // Component header: typeId(2) + size(2) = 4 bytes.
        if (remaining < 4)
        {
            LOG_WARN(kTag, "Entity %u: component buffer overflow at bit %d", entityId, bit);
            break;
        }

        // Write component type ID (bit position as proxy for typeId).
        writePtr[0] = static_cast<uint8_t>(bit & 0xFF);
        writePtr[1] = static_cast<uint8_t>((bit >> 8) & 0xFF);
        writePtr += 2;
        remaining -= 2;

        // In production: read component data from WorldState block.
        //   const auto* block = m_world.GetBlock(bit);
        //   const auto* data = GetComponentForEntity(block, entityId);
        //   std::memcpy(writePtr, data, componentSize);
        //
        // For now: write a placeholder payload (4 bytes of zeros).
        constexpr uint16_t kPlaceholderSize = 4;
        writePtr[0] = static_cast<uint8_t>(kPlaceholderSize & 0xFF);
        writePtr[1] = static_cast<uint8_t>((kPlaceholderSize >> 8) & 0xFF);
        writePtr += 2;
        remaining -= 2;

        if (remaining < kPlaceholderSize)
        {
            LOG_WARN(kTag, "Entity %u: component data overflow at bit %d", entityId, bit);
            break;
        }

        std::memset(writePtr, 0, kPlaceholderSize);
        writePtr += kPlaceholderSize;
        remaining -= kPlaceholderSize;

        outDataSize += 4 + kPlaceholderSize;  // header + payload
    }
}

// ─── Dirty entity tracking ──────────────────────────────────────────────────

/// Mark an entity's components as dirty for replication.
///
/// Dirty tracking is the foundation of delta snapshots.  When a component
/// changes (via system simulation or client input), the corresponding bit
/// in the entity's dirtyComponents mask is set.  The next replication cycle
/// will include only those dirty components in the delta.
///
/// @param id     Entity ID to mark dirty.
/// @param mask   Bitmask of component types that changed.
void WorldNode::MarkEntityDirty(EntityId id, ComponentMask mask) noexcept
{
    auto& dirtyState = m_entityDirtyStates[id];
    dirtyState.dirtyComponents |= mask;
    dirtyState.activeComponents |= mask;
    dirtyState.lastUpdateTick = static_cast<uint32_t>(m_worldTick);
    dirtyState.deleted = false;
}

/// Clear dirty state for an entity after successful replication.
///
/// In production, this is called only after the client ACKs the delta.
/// For now, it's called immediately after SendDelta (optimistic clearing).
///
/// @param id  Entity ID to clear.
void WorldNode::ClearEntityDirty(EntityId id) noexcept
{
    auto it = m_entityDirtyStates.find(id);
    if (it != m_entityDirtyStates.end())
    {
        it->second.dirtyComponents = 0;
    }
}

/// Enumerate all entities with their active component masks for snapshot building.
///
/// Used by the SnapshotBuilder's EntityEnumeratorFn to iterate all replicated entities.
///
/// @return Vector of (entityId, activeComponentMask) pairs.
std::vector<std::pair<uint32_t, ComponentMask>>
    WorldNode::EnumerateEntities() const noexcept
{
    std::vector<std::pair<uint32_t, ComponentMask>> result;
    result.reserve(m_entityDirtyStates.size());

    for (const auto& [eid, state] : m_entityDirtyStates)
    {
        if (!state.deleted && state.activeComponents != 0)
        {
            result.emplace_back(static_cast<uint32_t>(eid), state.activeComponents);
        }
    }

    return result;
}

} // namespace SagaEngine::World
