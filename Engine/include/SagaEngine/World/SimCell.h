/// @file SimCell.h
/// @brief Dynamic simulation cell — the fundamental unit of world simulation.
///
/// Layer  : SagaEngine / World
/// Purpose: Unlike static "zones", a SimCell is a living computation unit:
///          - Can split when entity density exceeds threshold
///          - Can merge with neighbors when density drops
///          - Can migrate to another compute node for load balancing
///          - Owns a spatial footprint (grid coord or AABB)
///          - Contains entities that participate in domain simulations
///
/// Design rules:
///   - Cells are identified by SimCellId (world-coord + generation)
///   - Cell state machine: Dormant → Loading → Active → {Splitting | Merging | Migrating} → Dormant
///   - Cells register with one or more simulation domains (Combat, Economy, Ecology)
///   - Entity lifecycle is managed at the cell level
///   - Cells communicate via EventStream (not direct method calls)
///
/// What this is NOT:
///   - Not a "zone". Zones are static. Cells are dynamic.
///   - Not an ECS world. Cells contain entities but simulation runs via domains.
///   - Not a network endpoint. Migration is handled by WorldNode.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/ECS/ComponentRegistry.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <functional>
#include <chrono>

namespace SagaEngine::World {

using ECS::EntityId;
using Math::Vec3;

// ─── Cell identity ─────────────────────────────────────────────────────────────

/// A cell is identified by its world grid coordinate and a generation number.
/// The generation increments on every split/merge/migrate so that stale
/// references from other nodes can detect the cell has been replaced.
struct SimCellId
{
    int16_t worldX  = 0;       ///< Grid column (cell units).
    int16_t worldZ  = 0;       ///< Grid row    (cell units).
    uint16_t gen    = 0;       ///< Generation — bumps on structural change.

    [[nodiscard]] constexpr bool operator==(const SimCellId& o) const noexcept
    {
        return worldX == o.worldX && worldZ == o.worldZ && gen == o.gen;
    }

    [[nodiscard]] constexpr bool operator!=(const SimCellId& o) const noexcept
    {
        return !(*this == o);
    }

    [[nodiscard]] constexpr bool SameCoord(const SimCellId& o) const noexcept
    {
        return worldX == o.worldX && worldZ == o.worldZ;
    }
};

// ─── Cell footprint ────────────────────────────────────────────────────────────

/// Spatial extent of a cell.  AABB is the simplest; future versions may use
/// irregular shapes for adaptive partitioning.
struct SimCellFootprint
{
    Vec3 centre     = Vec3::Zero();   ///< Centre of the cell in world space.
    Vec3 halfExtents = Vec3(32.0f, 256.0f, 32.0f); ///< Half-size (default 64×512×64).

    [[nodiscard]] bool ContainsPoint(const Vec3& p) const noexcept
    {
        const Vec3 d = p - centre;
        return std::abs(d.x) <= halfExtents.x &&
               std::abs(d.y) <= halfExtents.y &&
               std::abs(d.z) <= halfExtents.z;
    }

    [[nodiscard]] float DistanceSqToPoint(const Vec3& p) const noexcept
    {
        const Vec3 d = p - centre;
        const float dx = std::max(0.0f, std::abs(d.x) - halfExtents.x);
        const float dz = std::max(0.0f, std::abs(d.z) - halfExtents.z);
        return dx * dx + dz * dz;
    }
};

// ─── Cell state machine ────────────────────────────────────────────────────────

enum class SimCellState : uint8_t
{
    Dormant,        ///< Not yet loaded; no entities, no simulation.
    Loading,        ///< Assets and entity data being streamed in.
    Active,         ///< Fully operational; running domain ticks.
    Splitting,      ///< Density exceeded; dividing into children.
    Merging,        ///< Density dropped; absorbing into neighbour.
    Migrating,      ///< Moving to another compute node.
};

[[nodiscard]] inline const char* SimCellStateToString(SimCellState s) noexcept
{
    switch (s)
    {
        case SimCellState::Dormant:    return "Dormant";
        case SimCellState::Loading:    return "Loading";
        case SimCellState::Active:     return "Active";
        case SimCellState::Splitting:  return "Splitting";
        case SimCellState::Merging:    return "Merging";
        case SimCellState::Migrating:  return "Migrating";
    }
    return "Unknown";
}

// ─── Simulation domain registration ────────────────────────────────────────────

/// A cell can participate in multiple simulation domains simultaneously.
/// Each domain runs at its own tick rate.
enum class SimDomainType : uint8_t
{
    Combat,      ///< 60hz — projectiles, cooldowns, hit detection.
    Movement,    ///< 20hz — position updates, collision checks.
    Economy,     ///< 1hz  — market prices, resource production.
    Politics,    ///< 1/min — guild influence, territory control.
    Ecology,     ///< 1/hr  — weather, crop growth, NPC migration.
    Narrative,   ///< Event-driven — story triggers, NPC dialogue.
    Count,
};

/// Bitmask of domains a cell is registered with.
using SimDomainMask = uint8_t;

inline constexpr SimDomainMask SimDomainMaskFor(SimDomainType d) noexcept
{
    return static_cast<SimDomainMask>(1u << static_cast<uint8_t>(d));
}

// ─── Cell metrics (for split/merge/migrate decisions) ──────────────────────────

struct SimCellMetrics
{
    uint32_t entityCount       = 0;  ///< Live entities in this cell.
    uint32_t playerCount       = 0;  ///< Human players (affects priority).
    uint32_t npcCount          = 0;  ///< AI agents.
    float    cpuLoadPct        = 0.0f; ///< 0–100, per-node estimate.
    float    memoryMB          = 0.0f; ///< Resident memory for this cell's data.
    uint64_t lastActivityTick  = 0;  ///< Monotonic tick of last state change.
};

// ─── Cell events (appended to EventStream) ─────────────────────────────────────

/// Every structural change to a cell is recorded as an immutable event.
enum class SimCellEventType : uint8_t
{
    Created,        ///< New cell spawned at coord.
    EntitiesLoaded, ///< Asset streaming complete; entities materialized.
    TickDomain,     ///< A domain simulation ran (combat, economy, etc.).
    EntitySpawned,  ///< New entity added to cell.
    EntityDespawned,///< Entity removed from cell.
    SplitInitiated, ///< Cell density exceeded threshold.
    SplitCompleted, ///< Children created; this cell dissolved.
    MergeInitiated, ///< Cell density dropped below threshold.
    MergeCompleted, ///< Absorbed by neighbour.
    MigrateStarted, ///< Cell data sent to target node.
    MigrateComplete,///< Ownership transferred; source dormant.
};

struct SimCellEvent
{
    SimCellEventType type       = SimCellEventType::Created;
    uint64_t         timestamp  = 0;  ///< Wall-clock microseconds since epoch.
    uint64_t         worldTick  = 0;  ///< Global world tick at time of event.
    EntityId         entityId   = 0;  ///< Affected entity (0 = cell-level event).
    uint32_t         data       = 0;  ///< Type-specific payload (domain index, etc.).
};

// ─── Thresholds ────────────────────────────────────────────────────────────────

struct SimCellThresholds
{
    uint32_t maxEntities     = 512;    ///< Split when entity count exceeds this.
    uint32_t minEntities     = 16;     ///< Merge candidate when below this for N ticks.
    uint32_t maxPlayers      = 128;    ///< Player-specific split threshold.
    uint64_t mergeGraceTicks = 300;    ///< Min ticks in Low state before merge (5s at 60hz).
    float    cpuLoadTarget   = 70.0f;  ///< Target CPU% per node; cells above migrate.
};

// ─── SimCell ───────────────────────────────────────────────────────────────────

/// A single dynamic simulation cell.
///
/// Lifecycle:
///   Created(id, footprint) → Load() → Activate() → [Tick() × N] → Destroy()
///
/// Structural changes (split/merge/migrate) transition through intermediate
/// states so that other cells and the WorldNode can react consistently.
///
/// Thread model:
///   A SimCell is ticked by the DomainScheduler on the owning WorldNode's
///   simulation thread.  Cross-cell queries go through the RelevanceGraph,
///   not direct method calls.
class SimCell
{
public:
    SimCell() = default;

    explicit SimCell(SimCellId id, SimCellFootprint footprint) noexcept
        : m_id(id), m_footprint(std::move(footprint))
    {
    }

    // ─── Identity ─────────────────────────────────────────────────────────────

    [[nodiscard]] SimCellId               Id()        const noexcept { return m_id; }
    [[nodiscard]] const SimCellFootprint& Footprint()  const noexcept { return m_footprint; }
    [[nodiscard]] SimCellState            State()     const noexcept { return m_state; }

    // ─── Domain registration ──────────────────────────────────────────────────

    /// Register this cell with one or more simulation domains.
    void RegisterDomains(SimDomainMask mask) noexcept
    {
        m_domains |= mask;
    }

    /// Unregister from domains.  Entities are NOT removed — they continue
    /// to exist but are no longer simulated by those domains.
    void UnregisterDomains(SimDomainMask mask) noexcept
    {
        m_domains &= ~mask;
    }

    [[nodiscard]] SimDomainMask RegisteredDomains() const noexcept { return m_domains; }

    // ─── Entity management ────────────────────────────────────────────────────

    /// Add an entity to this cell.  The entity must not already be in a cell.
    void AddEntity(EntityId id) noexcept;

    /// Remove an entity from this cell.  No-op if not present.
    void RemoveEntity(EntityId id) noexcept;

    /// Return true if the entity is in this cell.
    [[nodiscard]] bool HasEntity(EntityId id) const noexcept;

    /// Return all entity IDs in this cell (for iteration by domain ticks).
    [[nodiscard]] const std::unordered_set<EntityId>& Entities() const noexcept
    {
        return m_entities;
    }

    [[nodiscard]] uint32_t EntityCount()    const noexcept { return m_metrics.entityCount; }
    [[nodiscard]] uint32_t PlayerCount()    const noexcept { return m_metrics.playerCount; }
    [[nodiscard]] uint32_t NpcCount()       const noexcept { return m_metrics.npcCount; }

    // ─── Metrics ──────────────────────────────────────────────────────────────

    [[nodiscard]] const SimCellMetrics& Metrics() const noexcept { return m_metrics; }
    [[nodiscard]] SimCellThresholds     Thresholds() const noexcept { return m_thresholds; }

    void SetThresholds(SimCellThresholds t) noexcept { m_thresholds = t; }

    /// Update metrics (call from domain tick or entity spawn/despawn).
    void UpdateMetrics() noexcept;

    // ─── Split / merge decisions ──────────────────────────────────────────────

    /// Return true if this cell should split (density > threshold).
    [[nodiscard]] bool ShouldSplit() const noexcept;

    /// Return true if this cell should merge with a neighbour.
    [[nodiscard]] bool ShouldMerge() const noexcept;

    // ─── Split / merge execution ──────────────────────────────────────────────

    /// Execute a split: divide entities between two child cells based on
    /// spatial position (X or Z axis split).  Returns the two new cell IDs.
    /// The original cell is left in Dormant state and should be removed.
    ///
    /// @param positionQuery  Callback to get an entity's world position.
    ///                       Returns false if the entity has no Transform.
    using PositionQueryFn = std::function<bool(EntityId, Vec3& outPosition)>;
    [[nodiscard]] std::pair<SimCellId, SimCellId>
        ExecuteSplit(PositionQueryFn positionQuery) const noexcept;

    /// Execute a merge: transfer all entities from this cell into the
    /// target cell.  The source cell is left empty and Dormant.
    void ExecuteMergeInto(SimCell& target) noexcept;

    // ─── Events ───────────────────────────────────────────────────────────────

    /// Append an event to this cell's event log.  The caller (WorldNode)
    /// is responsible for persisting the event to the EventStream.
    void AppendEvent(SimCellEvent evt) noexcept;

    /// Drain and return all events since the last call.  Used by the
    /// WorldNode to batch events to the EventStream.
    [[nodiscard]] std::vector<SimCellEvent> DrainEvents() noexcept;

    // ─── State transitions ────────────────────────────────────────────────────

    void SetState(SimCellState newState) noexcept;

private:
    SimCellId           m_id{};
    SimCellFootprint    m_footprint{};
    SimCellState        m_state   = SimCellState::Dormant;
    SimDomainMask       m_domains = 0;

    std::unordered_set<EntityId> m_entities;

    SimCellMetrics     m_metrics{};
    SimCellThresholds  m_thresholds{};

    mutable std::vector<SimCellEvent> m_events;
    uint64_t                  m_lowStateStartTick = 0;  ///< For merge grace tracking.
};

// ─── Helpers ─────────────────────────────────────────────────────────────────────

/// Convert world-space position to cell coord at the given cell size.
[[nodiscard]] inline SimCellId CoordFromWorld(float worldX, float worldZ,
                                                   float cellSize, uint16_t gen = 0) noexcept
{
    return SimCellId{
        static_cast<int16_t>(static_cast<int>(worldX / cellSize)),
        static_cast<int16_t>(static_cast<int>(worldZ / cellSize)),
        gen
    };
}

/// Squared distance from a world point to a cell's footprint.
[[nodiscard]] inline float DistanceSqCoordToWorldPoint(SimCellId coord,
                                                        float cellSize,
                                                        const Vec3& point) noexcept
{
    const float cx = (static_cast<float>(coord.worldX) + 0.5f) * cellSize;
    const float cz = (static_cast<float>(coord.worldZ) + 0.5f) * cellSize;
    const float dx = point.x - cx;
    const float dz = point.z - cz;
    const float hx = cellSize * 0.5f;
    const float hz = hx;
    const float ex = std::max(0.0f, std::abs(dx) - hx);
    const float ez = std::max(0.0f, std::abs(dz) - hz);
    return ex * ex + ez * ez;
}

} // namespace SagaEngine::World
