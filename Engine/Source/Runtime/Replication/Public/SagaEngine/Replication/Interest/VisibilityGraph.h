/// @file VisibilityGraph.h
/// @brief Uniform-grid spatial visibility graph for wide-area interest management.
///
/// Layer  : SagaServer / Networking / Interest
/// Purpose: Partitions world space into fixed-size cells so that per-tick
///          visibility queries — "which entities are within view of this
///          client?" — can run in O(k) over a bounded neighbourhood instead
///          of O(N) over every replicated entity. Each cell tracks its
///          resident entities and the clients subscribed to that cell.
///
/// Design:
///   - Cells are addressed by integer (cellX, cellZ) pairs (XZ-plane world,
///     Y ignored for broad-phase — vertical culling happens downstream).
///   - Entity membership is updated incrementally on position change.
///   - Client observers are subscribed to a square ring of cells centred on
///     the viewer — radius is measured in cells, not metres.
///   - All mutating operations are guarded by a shared_mutex so reader-heavy
///     workloads (scoring, per-tick relevancy) do not serialise.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Interest/InterestArea.h"
#include "SagaEngine/ECS/Entity.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SagaEngine::Networking::Interest
{

// ─── Cell Coordinates ─────────────────────────────────────────────────────────

/// Integer 2D cell index in the uniform grid (XZ plane).
struct CellCoord
{
    int32_t x{0};
    int32_t z{0};

    [[nodiscard]] bool operator==(const CellCoord& other) const noexcept
    {
        return x == other.x && z == other.z;
    }
};

/// Hash specialisation for CellCoord as an unordered_map key.
struct CellCoordHash
{
    [[nodiscard]] std::size_t operator()(const CellCoord& c) const noexcept
    {
        const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x));
        const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.z));
        return std::hash<std::uint64_t>{}((ux << 32) ^ uz);
    }
};

// ─── Visibility Cell ──────────────────────────────────────────────────────────

/// A single grid cell holding the entities that live inside it and the
/// set of clients currently subscribed to updates from this cell.
struct VisibilityCell
{
    CellCoord                    coord;            ///< Index of this cell in the grid.
    std::unordered_set<EntityId> entities;         ///< Entities whose position maps here.
    std::unordered_set<ClientId> subscribers;      ///< Clients observing this cell.
    std::uint64_t                lastUpdateTick{0};///< Monotonic tick of last mutation.

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return entities.empty() && subscribers.empty();
    }
};

// ─── Graph Configuration ──────────────────────────────────────────────────────

/// Static configuration applied to a VisibilityGraph instance at construction.
struct VisibilityGraphConfig
{
    float    cellSize{64.0f};       ///< World-unit size of a single grid cell.
    int32_t  defaultViewRadius{2};  ///< Default neighbour ring radius, in cells.
    int32_t  maxViewRadius{8};      ///< Upper bound on per-client view radius.
    std::size_t reserveCells{1024}; ///< Initial hash bucket reservation.
};

// ─── Graph Statistics ─────────────────────────────────────────────────────────

/// Snapshot of aggregate graph state — cheap to build, safe to copy.
struct VisibilityGraphStats
{
    std::size_t totalCells{0};
    std::size_t trackedEntities{0};
    std::size_t trackedClients{0};
    std::size_t totalSubscriptions{0};
    std::uint64_t lastRebuildTick{0};
};

// ─── VisibilityGraph ──────────────────────────────────────────────────────────

/// Spatial grid backing the broad-phase of interest management.
///
/// Threading: public API is safe to call from multiple simulation threads.
/// Read-only queries acquire a shared lock; mutations acquire an exclusive
/// lock. Callers must not hold the graph lock while dispatching replication
/// callbacks — copy the result vectors first.
class VisibilityGraph final
{
public:
    /// Callback fired when an entity enters a client's view for the first time.
    using OnEntityVisibleFn   = std::function<void(ClientId, EntityId)>;
    /// Callback fired when an entity leaves every cell the client observes.
    using OnEntityInvisibleFn = std::function<void(ClientId, EntityId)>;

    explicit VisibilityGraph(VisibilityGraphConfig config = {});
    ~VisibilityGraph() = default;

    VisibilityGraph(const VisibilityGraph&)            = delete;
    VisibilityGraph& operator=(const VisibilityGraph&) = delete;

    // ── Entity lifecycle ──────────────────────────────────────────────────────

    /// Insert or move an entity to the cell corresponding to `position`.
    void UpdateEntity(EntityId entity, const Vector3& position);

    /// Remove an entity from any cell it occupies.
    void RemoveEntity(EntityId entity);

    // ── Client lifecycle ──────────────────────────────────────────────────────

    /// Place `client` at `position` and subscribe it to the surrounding ring
    /// of cells (radius taken from config if < 0 is supplied).
    void UpdateClient(ClientId client, const Vector3& position, int32_t viewRadius = -1);

    /// Fully remove a client and all of its cell subscriptions.
    void RemoveClient(ClientId client);

    // ── Queries ───────────────────────────────────────────────────────────────

    /// Return every entity visible to `client` based on its current subscriptions.
    [[nodiscard]] std::vector<EntityId> QueryVisibleEntities(ClientId client) const;

    /// Return every entity inside the single cell containing `position`.
    [[nodiscard]] std::vector<EntityId> QueryEntitiesInCell(const Vector3& position) const;

    /// Return every entity inside the square neighbourhood of `position`.
    [[nodiscard]] std::vector<EntityId> QueryEntitiesInRadius(const Vector3& position,
                                                              int32_t        viewRadius) const;

    /// Return every client subscribed to any cell containing `entity`.
    [[nodiscard]] std::vector<ClientId> QueryObserversOf(EntityId entity) const;

    // ── Bookkeeping ───────────────────────────────────────────────────────────

    /// Clear every cell, entity, and client; retains configuration and callbacks.
    void Clear();

    /// Remove cells that contain neither entities nor subscribers.
    /// Returns the number of cells freed.
    std::size_t PruneEmptyCells();

    /// Advance the monotonic tick counter used to stamp cell mutations.
    void Tick(std::uint64_t serverTick);

    // ── Callbacks ─────────────────────────────────────────────────────────────

    void SetOnEntityVisible(OnEntityVisibleFn cb)     { m_OnEntityVisible   = std::move(cb); }
    void SetOnEntityInvisible(OnEntityInvisibleFn cb) { m_OnEntityInvisible = std::move(cb); }

    // ── Introspection ─────────────────────────────────────────────────────────

    [[nodiscard]] VisibilityGraphStats GetStatistics() const;
    [[nodiscard]] const VisibilityGraphConfig& GetConfig() const noexcept { return m_Config; }

    /// Convert a world position to the cell coordinate that owns it.
    [[nodiscard]] CellCoord CellFromPosition(const Vector3& position) const noexcept;

private:
    // ── Internals ─────────────────────────────────────────────────────────────

    using CellTable    = std::unordered_map<CellCoord, VisibilityCell, CellCoordHash>;
    using EntityIndex  = std::unordered_map<EntityId, CellCoord>;
    using ClientRecord = std::unordered_map<ClientId, std::unordered_set<CellCoord, CellCoordHash>>;

    /// Get an existing cell or lazily create one at `coord`. Requires exclusive lock.
    [[nodiscard]] VisibilityCell& TouchCell(const CellCoord& coord);

    /// Detach an entity from the cell referenced by its last known coord.
    /// Requires exclusive lock.
    void DetachEntityLocked(EntityId entity);

    /// Compute the visible set for a client without re-acquiring the lock.
    /// Requires at least a shared lock.
    [[nodiscard]] std::vector<EntityId> CollectVisibleLocked(ClientId client) const;

    /// Fire visibility transition callbacks for the diff between two subscription sets.
    void DispatchTransitionsLocked(ClientId                                        client,
                                   const std::unordered_set<CellCoord, CellCoordHash>& previous,
                                   const std::unordered_set<CellCoord, CellCoordHash>& next);

    VisibilityGraphConfig m_Config;

    mutable std::shared_mutex m_Mutex;
    CellTable                 m_Cells;
    EntityIndex               m_EntityIndex;
    ClientRecord              m_ClientSubscriptions;
    std::uint64_t             m_CurrentTick{0};

    OnEntityVisibleFn   m_OnEntityVisible;
    OnEntityInvisibleFn m_OnEntityInvisible;
};

} // namespace SagaEngine::Networking::Interest
