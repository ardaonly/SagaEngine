/// @file InterestManagement.h
/// @brief Zone-partitioned relevancy system for MMO-scale replication.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Filters the set of entities that a client needs to track
///          based on spatial proximity and relevance.  Without interest
///          management, every client receives updates for every entity
///          in the world — an approach that collapses at MMO scale.
///
/// Design rules:
///   - Uniform grid partition with configurable cell size.
///   - Each entity is mapped to a grid cell based on its position.
///   - A client subscribes to cells within its interest radius.
///   - Entity updates are only sent if the entity is in a subscribed cell.
///   - Hysteresis on cell subscription to prevent thrashing at boundaries.

#pragma once

#include "SagaEngine/ECS/Entity.h"
#include "SagaEngine/Math/Vec3.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Grid coordinates ──────────────────────────────────────────────────────

/// Integer grid cell coordinate.
struct CellCoord
{
    std::int32_t x = 0;
    std::int32_t y = 0;

    bool operator==(const CellCoord& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

/// Hash function for CellCoord.
struct CellCoordHash
{
    std::size_t operator()(const CellCoord& c) const noexcept
    {
        // Combine x and y into a single hash value.
        std::size_t h = static_cast<std::size_t>(c.x);
        h ^= static_cast<std::size_t>(c.y) + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

// ─── Interest config ───────────────────────────────────────────────────────

struct InterestConfig
{
    /// Size of one grid cell in world units.
    float cellSize = 64.0f;

    /// Number of cells in each direction from the client's cell that
    /// are considered relevant.  A radius of 2 means a 5x5 block.
    std::uint32_t interestRadius = 2;

    /// Number of extra cells beyond the interest radius that are
    /// subscribed to prevent thrashing at cell boundaries.
    std::uint32_t hysteresisMargin = 1;
};

// ─── Interest manager ──────────────────────────────────────────────────────

/// Client-side interest management filter.
///
/// Tracks which entities are relevant to this client based on spatial
/// partitioning.  The replication pipeline consults this filter before
/// applying each delta to skip entities outside the client's interest
/// area.
class InterestManager
{
public:
    InterestManager() = default;
    ~InterestManager() = default;

    InterestManager(const InterestManager&)            = delete;
    InterestManager& operator=(const InterestManager&) = delete;

    /// Configure the interest system.  Must be called before use.
    void Configure(InterestConfig config) noexcept;

    /// Update the client's position.  This recalculates the set of
    /// subscribed cells and triggers hysteresis-based subscription.
    void UpdateClientPosition(const Math::Vec3& position) noexcept;

    /// Register an entity at a given world position.
    void RegisterEntity(ECS::EntityId entityId, const Math::Vec3& position) noexcept;

    /// Unregister an entity.
    void UnregisterEntity(ECS::EntityId entityId) noexcept;

    /// Update an entity's position.
    void UpdateEntityPosition(ECS::EntityId entityId, const Math::Vec3& position) noexcept;

    /// Return true if the entity is within the client's interest area.
    [[nodiscard]] bool IsRelevant(ECS::EntityId entityId) const noexcept;

    /// Return the set of currently relevant entity IDs.
    [[nodiscard]] std::unordered_set<ECS::EntityId> GetRelevantEntities() const noexcept;

    /// Return the number of relevant entities (for diagnostics).
    [[nodiscard]] std::size_t RelevantCount() const noexcept;

    /// Convert a world position to a grid cell coordinate.
    [[nodiscard]] CellCoord WorldToCell(const Math::Vec3& position) const noexcept;

private:
    InterestConfig config_{};

    /// Client's current cell.
    CellCoord clientCell_{};

    /// Set of currently subscribed cells.
    std::unordered_set<CellCoord, CellCoordHash> subscribedCells_;

    /// Map from entity ID to its grid cell.
    std::unordered_map<ECS::EntityId, CellCoord> entityCells_;

    /// Map from cell coordinate to the set of entities in that cell.
    std::unordered_map<CellCoord, std::unordered_set<ECS::EntityId>, CellCoordHash> cellEntities_;
};

} // namespace SagaEngine::Client::Replication
