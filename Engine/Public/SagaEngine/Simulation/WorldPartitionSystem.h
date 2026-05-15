/// @file WorldPartitionSystem.h
/// @brief Zone-partitioned world system with boundary crossing events and spatial indexing.
///
/// Layer  : SagaEngine / Simulation / World
/// Purpose: Divides the game world into manageable zones and cells for
///          efficient spatial queries, replication filtering, and streaming.
///          This system replaces the flat InterestManager grid with a
///          hierarchical structure that scales to open-world MMO dimensions.
///
/// Design:
///   - World is divided into Zones (large areas, e.g. 1024x1024 units).
///   - Each Zone contains Cells (smaller grid, e.g. 64x64 units).
///   - Entities are tracked in their current cell; movement triggers migration.
///   - Boundary crossing events fire when entities move between cells/zones.
///   - Zone transition triggers allow gameplay hooks (loading, visibility, etc).
///
/// Thread safety: NOT thread-safe. Call from simulation thread only.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Core/Events/EventBus.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>

namespace SagaEngine::Simulation {

// ─── Partition identifiers ──────────────────────────────────────────────────

/// Unique zone identifier.
struct ZoneId
{
    std::int32_t worldX = 0;
    std::int32_t worldZ = 0;

    bool operator==(const ZoneId& other) const noexcept
    {
        return worldX == other.worldX && worldZ == other.worldZ;
    }

    bool operator!=(const ZoneId& other) const noexcept { return !(*this == other); }
};

/// Hash for ZoneId.
struct ZoneIdHash
{
    std::size_t operator()(ZoneId id) const noexcept
    {
        std::size_t h = static_cast<std::size_t>(id.worldX);
        h ^= static_cast<std::size_t>(id.worldZ) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

/// Cell coordinate within a zone.
struct CellCoord
{
    std::int16_t localX = 0;
    std::int16_t localZ = 0;

    bool operator==(const CellCoord& other) const noexcept
    {
        return localX == other.localX && localZ == other.localZ;
    }

    bool operator!=(const CellCoord& other) const noexcept { return !(*this == other); }
};

/// Hash for CellCoord.
struct CellCoordHash
{
    std::size_t operator()(CellCoord c) const noexcept
    {
        std::size_t h = static_cast<std::size_t>(static_cast<std::int32_t>(c.localX));
        h ^= static_cast<std::size_t>(static_cast<std::int32_t>(c.localZ)) +
             0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

/// Hash for EntityId (uint32_t identity hash).
struct EntityIdHash
{
    std::size_t operator()(ECS::EntityId id) const noexcept
    {
        return static_cast<std::size_t>(id);
    }
};

/// Global cell identifier (zone + local coord).
struct GlobalCellId
{
    ZoneId    zone;
    CellCoord local;

    bool operator==(const GlobalCellId& other) const noexcept
    {
        return zone == other.zone && local == other.local;
    }

    bool operator!=(const GlobalCellId& other) const noexcept { return !(*this == other); }
};

/// Hash for GlobalCellId.
struct GlobalCellIdHash
{
    std::size_t operator()(GlobalCellId id) const noexcept
    {
        std::size_t h = ZoneIdHash{}(id.zone);
        h ^= CellCoordHash{}(id.local) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

// ─── Partition events ───────────────────────────────────────────────────────

/// Fired when an entity crosses a cell boundary.
struct EntityCellCrossingEvent
{
    ECS::EntityId  entityId;
    GlobalCellId   fromCell;
    GlobalCellId   toCell;
    Math::Vec3     newPosition;
};

/// Fired when an entity crosses a zone boundary.
struct EntityZoneCrossingEvent
{
    ECS::EntityId  entityId;
    ZoneId         fromZone;
    ZoneId         toZone;
    Math::Vec3     newPosition;
};

/// Fired when an entity enters a cell for the first time.
struct EntityCellEnterEvent
{
    ECS::EntityId  entityId;
    GlobalCellId   cell;
    Math::Vec3     position;
};

/// Fired when an entity leaves a cell entirely.
struct EntityCellLeaveEvent
{
    ECS::EntityId  entityId;
    GlobalCellId   cell;
    Math::Vec3     position;
};

// ─── Zone configuration ─────────────────────────────────────────────────────

struct ZoneConfig
{
    /// Size of one zone in world units (default 1024).
    float zoneSize = 1024.f;

    /// Number of cells per zone along each axis (default 16 = 16x16 grid).
    std::uint32_t cellsPerZoneAxis = 16;

    /// Interest radius in cells for replication (default 2 = 5x5 area).
    std::uint32_t interestRadiusCells = 2;

    /// Hysteresis margin: extra cells subscribed at boundaries.
    std::uint32_t hysteresisMarginCells = 1;
};

// ─── Spatial index: Quadtree node ───────────────────────────────────────────

/// Lightweight quadtree node for spatial queries within a cell.
/// Used for fine-grained proximity queries beyond the grid resolution.
struct QuadtreeNode
{
    struct Bounds
    {
        float minX = 0.f, minZ = 0.f;
        float maxX = 0.f, maxZ = 0.f;

        [[nodiscard]] bool Contains(float x, float z) const noexcept
        {
            return x >= minX && x <= maxX && z >= minZ && z <= maxZ;
        }

        [[nodiscard]] bool Intersects(const Bounds& other) const noexcept
        {
            return maxX >= other.minX && minX <= other.maxX &&
                   maxZ >= other.minZ && minZ <= other.maxZ;
        }
    };

    Bounds bounds;
    std::array<std::unique_ptr<QuadtreeNode>, 4> children;
    std::vector<ECS::EntityId> entities;

    static constexpr std::size_t kMaxEntitiesPerNode = 16;
    static constexpr std::size_t kMaxDepth = 4;

    [[nodiscard]] bool IsLeaf() const noexcept { return children[0] == nullptr; }
};

// ─── Cell data ──────────────────────────────────────────────────────────────

/// Runtime data for one cell.
struct PartitionCell
{
    /// Entities currently in this cell.
    std::unordered_set<ECS::EntityId> entities;

    /// Optional quadtree for fine-grained spatial queries.
    std::unique_ptr<QuadtreeNode> quadtree;

    /// Last tick this cell was queried (for interest management).
    std::uint64_t lastQueriedTick = 0;
};

// ─── Zone data ──────────────────────────────────────────────────────────────

/// Runtime data for one zone.
struct PartitionZone
{
    ZoneId id;
    std::unordered_map<CellCoord, PartitionCell, CellCoordHash> cells;

    /// Entities that exist in this zone (cached for fast iteration).
    std::unordered_set<ECS::EntityId> allEntitiesInZone;
};

// ─── World Partition System ─────────────────────────────────────────────────

/// Hierarchical world partition system with boundary events and spatial indexing.
///
/// Manages entity placement across zones and cells, fires crossing events,
/// and provides spatial query interfaces for replication and gameplay.
class WorldPartitionSystem
{
public:
    WorldPartitionSystem() = default;
    ~WorldPartitionSystem() = default;

    WorldPartitionSystem(const WorldPartitionSystem&)            = delete;
    WorldPartitionSystem& operator=(const WorldPartitionSystem&) = delete;

    /// Configure the partition system. Must be called before use.
    void Configure(ZoneConfig config) noexcept;

    // ─── Entity tracking ──────────────────────────────────────────────────────

    /// Register an entity at a world position. Fires EntityCellEnterEvent.
    void RegisterEntity(ECS::EntityId entityId, const Math::Vec3& position);

    /// Unregister an entity. Fires EntityCellLeaveEvent if it was in a cell.
    void UnregisterEntity(ECS::EntityId entityId);

    /// Update an entity's position. Fires crossing events if cell/zone changed.
    void UpdateEntityPosition(ECS::EntityId entityId, const Math::Vec3& position);

    /// Return the current cell of an entity, or nullopt if unregistered.
    [[nodiscard]] std::optional<GlobalCellId> GetEntityCell(ECS::EntityId entityId) const noexcept;

    /// Return the current zone of an entity, or nullopt if unregistered.
    [[nodiscard]] std::optional<ZoneId> GetEntityZone(ECS::EntityId entityId) const noexcept;

    // ─── Spatial queries ──────────────────────────────────────────────────────

    /// Return all entities in the same cell as the given entity.
    [[nodiscard]] std::vector<ECS::EntityId> GetEntitiesInSameCell(ECS::EntityId entityId) const;

    /// Return all entities within interest radius of the given entity.
    [[nodiscard]] std::vector<ECS::EntityId> GetEntitiesInInterestRange(
        ECS::EntityId entityId) const;

    /// Return all entities in a specific cell.
    [[nodiscard]] std::vector<ECS::EntityId> GetEntitiesInCell(
        const GlobalCellId& cellId) const;

    /// Return all entities in a radius around a world position.
    [[nodiscard]] std::vector<ECS::EntityId> GetEntitiesInRadius(
        const Math::Vec3& center, float radius) const;

    // ─── Zone/cell conversion ─────────────────────────────────────────────────

    /// Convert world position to zone ID.
    [[nodiscard]] ZoneId WorldToZone(const Math::Vec3& position) const noexcept;

    /// Convert world position to cell coordinate within a zone.
    [[nodiscard]] CellCoord WorldToCellLocal(const Math::Vec3& position) const noexcept;

    /// Convert world position to global cell ID.
    [[nodiscard]] GlobalCellId WorldToGlobalCell(const Math::Vec3& position) const noexcept;

    // ─── Zone transition triggers ─────────────────────────────────────────────

    /// Register a callback for zone entry events.
    using ZoneEnterFn = std::function<void(ECS::EntityId, const ZoneId&)>;
    void OnZoneEnter(ZoneEnterFn fn);

    /// Register a callback for zone exit events.
    using ZoneExitFn = std::function<void(ECS::EntityId, const ZoneId&)>;
    void OnZoneExit(ZoneExitFn fn);

    // ─── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t RegisteredEntityCount() const noexcept;
    [[nodiscard]] std::size_t LoadedZoneCount() const noexcept;
    [[nodiscard]] std::size_t LoadedCellCount() const noexcept;
    [[nodiscard]] const ZoneConfig& GetConfig() const noexcept { return config_; }

private:
    ZoneConfig config_{};

    /// Entity -> current global cell mapping.
    std::unordered_map<ECS::EntityId, GlobalCellId, EntityIdHash> entityCells_;

    /// Loaded zones (only zones with registered entities exist).
    std::unordered_map<ZoneId, PartitionZone, ZoneIdHash> zones_;

    /// Event bus for partition events.
    Core::EventBus* eventBus_ = nullptr;

    /// Zone transition callbacks.
    std::vector<ZoneEnterFn> zoneEnterCallbacks_;
    std::vector<ZoneExitFn> zoneExitCallbacks_;

    // ─── Internal helpers ─────────────────────────────────────────────────────

    /// Get or create a zone.
    PartitionZone& GetOrCreateZone(const ZoneId& zoneId);

    /// Get or create a cell within a zone.
    PartitionCell& GetOrCreateCell(const ZoneId& zoneId, const CellCoord& cellCoord);

    /// Move an entity to a new cell, firing events as needed.
    void MoveEntityToCell(ECS::EntityId entityId,
                          const GlobalCellId& fromCell,
                          const GlobalCellId& toCell,
                          const Math::Vec3& position);

    /// Collect entities in a rectangular area of cells.
    [[nodiscard]] std::vector<ECS::EntityId> CollectEntitiesInCellRange(
        const ZoneId& zoneId,
        std::int16_t minCellX, std::int16_t minCellZ,
        std::int16_t maxCellX, std::int16_t maxCellZ) const;

    /// Compute cell size from zone config.
    [[nodiscard]] float CellSize() const noexcept;
};

} // namespace SagaEngine::Simulation
