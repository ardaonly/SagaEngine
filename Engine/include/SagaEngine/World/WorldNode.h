/// @file WorldNode.h
/// @brief World Kernel node — the server process that owns cells, domains, and replication.
///
/// Layer  : SagaEngine / World
/// Purpose: The WorldNode replaces the old ZoneServer as the primary server
///          process.  Unlike ZoneServer (which is "a server that carries
///          players"), the WorldNode is "a compute node in the world kernel"
///          that happens to serve clients.
///
///          A WorldNode owns:
///            - SimCells: dynamic simulation cells (not static zones)
///            - DomainScheduler: multi-rate tick domains
///            - EventStream: append-only event log
///            - RelevanceGraph: context-based interest
///            - Client sessions: connected players
///            - Replication: snapshot + delta delivery to clients
///
///          ZoneServer becomes a compatibility wrapper around WorldNode
///          during the transition period.
///
/// Architecture:
///   Edge Gateway (Session Mesh)
///     └─ Client sessions → WorldNode
///
///   Simulation Fabric
///     ├─ SimCell × N
///     ├─ DomainScheduler (multi-rate ticks)
///     ├─ RelevanceGraph (context-based interest)
///     └─ EventStream (event-sourced state)
///
///   State Backbone
///     ├─ EventStream → Snapshot Store
///     └─ Analytics Stream
///
/// Design rules:
///   - WorldNode is single-process for now.  Multi-node is a future extension.
///   - Cells can migrate between WorldNodes (future).
///   - The node runs on a fixed tick rate (60hz world tick).
///   - Domains fire at their own rates within the world tick.

#pragma once

#include "SagaEngine/World/SimCell.h"
#include "SagaEngine/World/DomainScheduler.h"
#include "SagaEngine/World/EventStream.h"
#include "SagaEngine/World/RelevanceGraph.h"
#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"
#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::World {

using ECS::EntityId;
using Simulation::WorldState;
using ServerTick = SagaEngine::Client::Replication::ServerTick;

// ─── Client session ─────────────────────────────────────────────────────────────

/// A connected client session.  The WorldNode tracks what each client
/// has seen so it can send deltas.
struct ClientSession
{
    uint64_t        sessionId         = 0;
    EntityId        controlledEntity  = 0;  ///< The player entity this client controls.
    ServerTick      lastAckedTick     = 0;  ///< Last snapshot the client acknowledged.
    uint64_t        lastSeenEventSeq  = 0;  ///< Last event sequence sent to this client.
    bool            wantsFullSnapshot = true; ///< True on connect, false after first snapshot.
    SimCellId       currentCell{};            ///< Cell the player is currently in.
};

// ─── WorldNode config ───────────────────────────────────────────────────────────

struct WorldNodeConfig
{
    std::string nodeId       = "world-node-01";
    float       worldTickHz  = 60.0f;    ///< World tick frequency.
    float       cellSize     = 64.0f;    ///< Default cell size in world units.
    uint32_t    maxClients   = 1024;     ///< Max connected clients.
    bool        headless     = true;     ///< Run without rendering.
};

// ─── WorldNode stats ────────────────────────────────────────────────────────────

struct WorldNodeStats
{
    uint64_t ticksCompleted      = 0;
    uint64_t cellsActive         = 0;
    uint64_t entitiesTotal       = 0;
    uint64_t clientsConnected    = 0;
    uint64_t snapshotsSent       = 0;
    uint64_t deltasSent          = 0;
    uint64_t eventsAppended      = 0;
    uint64_t relevanceQueries    = 0;
    float    tickDurationMs      = 0.0f;
    uint64_t lastTickWorldTick  = 0;
};

// ─── Domain tick dispatcher ─────────────────────────────────────────────────────

/// Called by the WorldNode when a domain fires for a cell.
/// The caller implements the actual domain simulation logic.
using DomainTickDispatcher = std::function<void(SimDomainType domain, SimCell& cell, float subTick)>;

// ─── WorldNode ──────────────────────────────────────────────────────────────────

/// World Kernel node — the central server process.
///
/// Lifecycle:
///   Init() → [Tick() × N] → Shutdown()
///
/// Each Tick():
///   1. Drain client input
///   2. Advance world by one tick
///   3. DomainScheduler fires domains at their rates
///   4. Dispatch domain simulation via callback
///   5. Update RelevanceGraph
///   6. Replicate state to clients (snapshots / deltas)
///   7. Drain events to EventStream
class WorldNode
{
public:
    WorldNode() = default;
    ~WorldNode() = default;

    WorldNode(const WorldNode&)            = delete;
    WorldNode& operator=(const WorldNode&) = delete;

    // ─── Lifecycle ─────────────────────────────────────────────────────────────

    bool Init(const WorldNodeConfig& config) noexcept;
    void Shutdown() noexcept;

    // ─── World tick ────────────────────────────────────────────────────────────

    /// Execute one world tick.  Called at worldTickHz (default 60hz).
    void Tick() noexcept;

    // ─── Cell management ──────────────────────────────────────────────────────

    /// Create or get a cell at the given coord.
    [[nodiscard]] SimCell* GetOrCreateCell(SimCellId id) noexcept;

    /// Get a cell by coord (returns nullptr if not loaded).
    [[nodiscard]] SimCell* GetCell(SimCellId id) noexcept;
    [[nodiscard]] const SimCell* GetCell(SimCellId id) const noexcept;

    /// Remove a cell (after merge or migration).
    void RemoveCell(SimCellId id) noexcept;

    /// Return all active cells.
    [[nodiscard]] const std::unordered_map<uint32_t, SimCell>& Cells() const noexcept
    {
        return m_cells;
    }

    // ─── Entity management ────────────────────────────────────────────────────

    /// Spawn an entity into a cell.
    void SpawnEntity(EntityId id, SimCellId cellId) noexcept;

    /// Despawn an entity.
    void DespawnEntity(EntityId id) noexcept;

    /// Move an entity between cells.
    void MoveEntity(EntityId id, SimCellId fromCell, SimCellId toCell) noexcept;

    /// Get the cell an entity is in.
    [[nodiscard]] SimCell* GetEntityCell(EntityId id) noexcept;

    // ─── Client management ────────────────────────────────────────────────────

    /// Add a connected client session.
    uint64_t AddClientSession(EntityId controlledEntity) noexcept;

    /// Remove a disconnected client.
    void RemoveClientSession(uint64_t sessionId) noexcept;

    /// Get a client session.
    [[nodiscard]] ClientSession* GetClientSession(uint64_t id) noexcept;

    // ─── Replication ──────────────────────────────────────────────────────────

    /// Send a snapshot or delta to a client.
    /// Returns true if data was sent.
    bool ReplicateToClient(uint64_t sessionId) noexcept;

    // ─── Event stream ─────────────────────────────────────────────────────────

    /// Append an event to the world's event stream.
    void AppendEvent(WorldEvent evt) noexcept;

    // ─── Domain registration ──────────────────────────────────────────────────

    /// Register a simulation domain with a tick callback.
    void RegisterDomain(SimDomainType type, const char* name,
                        uint32_t ticksPerSecond) noexcept;

    /// Set the domain tick dispatcher (called when domains fire).
    void SetDomainDispatcher(DomainTickDispatcher fn) noexcept
    {
        m_domainDispatcher = std::move(fn);
    }

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] const WorldNodeConfig&  Config()  const noexcept { return m_config; }
    [[nodiscard]] const WorldNodeStats&   Stats()   const noexcept { return m_stats; }
    [[nodiscard]] DomainScheduler&        Scheduler() noexcept { return m_scheduler; }
    [[nodiscard]] EventStream&            Events()  noexcept { return m_eventStream; }
    [[nodiscard]] RelevanceGraph&         Relevance() noexcept { return m_relevanceGraph; }
    [[nodiscard]] WorldState&             WorldStateRef() noexcept { return m_world; }

    [[nodiscard]] uint64_t WorldTick() const noexcept { return m_worldTick; }

private:
    // ─── Internal tick phases ──────────────────────────────────────────────────

    void DrainInput() noexcept;
    void StepSimulation() noexcept;
    void FireDomains() noexcept;
    void UpdateRelevance() noexcept;
    void ReplicateToClients() noexcept;
    void FlushEvents() noexcept;

    // ─── Cell coord packing ────────────────────────────────────────────────────

    [[nodiscard]] static uint32_t PackCellCoord(SimCellId id) noexcept
    {
        return (static_cast<uint32_t>(static_cast<uint16_t>(id.worldX)) << 16) |
               static_cast<uint32_t>(static_cast<uint16_t>(id.worldZ));
    }

    [[nodiscard]] static SimCellId UnpackCellCoord(uint32_t packed) noexcept
    {
        SimCellId id;
        id.worldX = static_cast<int16_t>(static_cast<uint16_t>(packed >> 16));
        id.worldZ = static_cast<int16_t>(static_cast<uint16_t>(packed & 0xFFFF));
        return id;
    }

    // ─── Members ──────────────────────────────────────────────────────────────

    WorldNodeConfig  m_config{};
    WorldNodeStats   m_stats{};

    uint64_t         m_worldTick = 0;

    // Cells: packed coord → SimCell
    std::unordered_map<uint32_t, SimCell> m_cells;

    // Entity → packed cell coord (for fast lookup).
    std::unordered_map<EntityId, uint32_t> m_entityCells;

    // Client sessions: session ID → ClientSession
    std::unordered_map<uint64_t, ClientSession> m_clients;
    uint64_t                                    m_nextSessionId = 1;

    // World state (ECS).
    WorldState m_world;

    // Subsystems.
    DomainScheduler   m_scheduler;
    EventStream       m_eventStream;
    RelevanceGraph    m_relevanceGraph;

    // Domain dispatch.
    DomainTickDispatcher m_domainDispatcher;
};

} // namespace SagaEngine::World
