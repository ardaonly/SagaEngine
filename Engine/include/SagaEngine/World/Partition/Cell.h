/// @file Cell.h
/// @brief Fixed-size spatial cell — the smallest unit of world partitioning.
///
/// Layer  : SagaEngine / World / Partition
/// Purpose: A `Cell` is the atomic unit the streaming subsystem operates
///          on.  The world is divided into a regular grid of cells with
///          an AABB footprint that matches the cell size, and every
///          mesh / texture / prop instance placed in the world is owned
///          by exactly one cell (the cell whose footprint contains the
///          instance's anchor point).  When the player moves, the
///          zone loader promotes cells from "dormant" to "loaded" as
///          they enter the apron, decoded assets are acquired through
///          the `ResourceManager`, and cells fade back out when the
///          player leaves.
///
///          The cell is deliberately a *descriptor*, not a runtime
///          container: it records which asset ids belong to the cell
///          and what its bounding box is, but it does not own any
///          decoded data.  The runtime residency of the assets lives
///          in `Resources::ResourceManager`; this class is what the
///          zone loader iterates when deciding what to prefetch next.
///
/// Design rules:
///   - Value semantics.  Cells are POD-ish descriptors and must be
///     cheap to copy; the zone loader streams them in and out of
///     hash tables constantly.
///   - Integer coordinates.  A cell is identified by its grid-space
///     `(x, z)` integer coordinate — floating-point world positions
///     are converted to cell coordinates via `CellCoordFromWorld`.
///     Integer keys survive cross-process migration (the shard mesh
///     needs to agree on cell ids) and never suffer FP drift.
///   - Asset list ownership.  The cell owns a small contiguous list
///     of the asset ids it contains so the loader can issue a single
///     batch prefetch instead of chasing per-instance pointers.
///   - Y-axis is vertical.  The grid is two-dimensional (X / Z) even
///     though the world is 3D; vertical partitioning is handled by
///     the render LOD layer rather than the cell grid.
///
/// What this header is NOT:
///   - Not a spatial index.  Per-entity queries go through
///     `VisibilityGraph`; cells are a streaming unit, not a broadphase.
///   - Not a chunk of memory.  The cell descriptor carries only asset
///     ids; decoded bytes live in `ResourceManager`.

#pragma once

#include "SagaEngine/Resources/StreamingRequest.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::World::Partition {

// ─── Cell coordinate ───────────────────────────────────────────────────────

/// Integer grid-space coordinate that identifies one cell in the
/// world partition.  Combined with the zone id, a `(zoneId, cellX,
/// cellZ)` triple uniquely identifies every cell in the entire world
/// and is stable across process restarts.
struct CellCoord
{
    std::int32_t x = 0; ///< Grid X (positive = east).
    std::int32_t z = 0; ///< Grid Z (positive = north).

    [[nodiscard]] constexpr bool operator==(const CellCoord& other) const noexcept
    {
        return x == other.x && z == other.z;
    }

    [[nodiscard]] constexpr bool operator!=(const CellCoord& other) const noexcept
    {
        return !(*this == other);
    }
};

/// Pack a cell coordinate into a 64-bit key for use in hash tables.
/// The packing is injective and preserves equality so two cells with
/// identical grid coordinates produce identical keys.
[[nodiscard]] constexpr std::uint64_t PackCellCoord(CellCoord coord) noexcept
{
    const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x));
    const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.z));
    return (ux << 32) | uz;
}

// ─── Cell state ────────────────────────────────────────────────────────────

/// Streaming lifecycle state of a cell.  The zone loader promotes
/// cells through these states in strict order and any transition is
/// observable to the diagnostics subsystem.
enum class CellState : std::uint8_t
{
    /// Cell is described in the registry but no assets are resident.
    /// This is the default state for every cell outside the apron.
    Dormant = 0,

    /// Prefetch has been queued and the streaming manager is working
    /// through the cell's asset list.  Callers treat the cell as
    /// "not yet usable" until it reaches `Loaded`.
    Prefetching,

    /// All critical assets are resident and the cell can be used
    /// for rendering and simulation.  This is the steady state while
    /// the player is inside the cell's apron.
    Loaded,

    /// The player has left the apron and the loader is holding the
    /// cell for a grace period before evicting its assets.  Assets
    /// remain resident so a quick return (e.g. back-tracking) does
    /// not retrigger a stream.
    Draining,
};

// ─── Cell footprint ────────────────────────────────────────────────────────

/// Axis-aligned bounding box for a cell in world space.  The footprint
/// is derived from the cell size and the cell coordinate; the loader
/// uses it to compute distance-based streaming priority.
struct CellFootprint
{
    float minX = 0.0f; ///< Minimum X corner (world-space metres).
    float minZ = 0.0f; ///< Minimum Z corner.
    float maxX = 0.0f; ///< Maximum X corner.
    float maxZ = 0.0f; ///< Maximum Z corner.
    float minY = 0.0f; ///< Vertical floor (used only for cull).
    float maxY = 0.0f; ///< Vertical ceiling.
};

// ─── Cell asset reference ──────────────────────────────────────────────────

/// One asset referenced by a cell.  The cell stores `(id, kind)`
/// pairs rather than bare ids because the `ResourceManager::Acquire`
/// call needs the kind to route the request to the right typed
/// streamer, and asking the registry for every single asset would
/// double the hash-lookup cost in the loader's hot path.
struct CellAssetRef
{
    SagaEngine::Resources::AssetId   assetId = 0;
    SagaEngine::Resources::AssetKind kind    =
        SagaEngine::Resources::AssetKind::Unknown;
};

// ─── Cell descriptor ───────────────────────────────────────────────────────

/// Descriptor for one partition cell.  The zone registry owns a
/// collection of these and the zone loader iterates them as the
/// player moves.
struct Cell
{
    /// Grid coordinate of the cell within its owning zone.
    CellCoord coord;

    /// World-space footprint in metres.  Redundant with `coord` plus
    /// the zone's cell size, but cached here so the loader does not
    /// recompute it on every distance test.
    CellFootprint footprint;

    /// Assets that live inside this cell.  Ordered from most to
    /// least critical so prefetch can fail fast when the budget is
    /// tight — the first few entries should always be the structural
    /// mesh and terrain material, and decorative props come last.
    std::vector<CellAssetRef> assetRefs;

    /// Current streaming state.  Written by the zone loader and
    /// read by every consumer that needs to know whether the cell
    /// is usable.
    CellState state = CellState::Dormant;

    /// Engine tick at which the cell last transitioned out of
    /// `Loaded` — used by the drain grace period to decide when
    /// eviction is safe.  Zero means "not yet drained".
    std::uint64_t lastDrainTick = 0;
};

// ─── Cell-size helpers ─────────────────────────────────────────────────────

/// Convert a world-space position to the grid coordinate of the cell
/// that contains it.  Negative positions round correctly (floor
/// semantics) so cells align at the origin rather than splitting
/// on the sign boundary.
[[nodiscard]] CellCoord CellCoordFromWorld(float worldX,
                                           float worldZ,
                                           float cellSizeMetres) noexcept;

/// Compute the footprint of a cell given its grid coordinate and
/// the shared cell size.  Vertical extents default to a wide band
/// that covers typical terrain; callers can override them if the
/// zone uses vertical partitioning.
[[nodiscard]] CellFootprint FootprintFromCoord(CellCoord coord,
                                               float     cellSizeMetres,
                                               float     minY = -256.0f,
                                               float     maxY =  1024.0f) noexcept;

/// Squared distance from a world-space XZ point to the nearest edge
/// of the cell footprint.  Returns zero if the point is inside.
/// Used by the loader to rank cells for prefetch priority.
[[nodiscard]] float DistanceSqToFootprint(const CellFootprint& footprint,
                                          float worldX,
                                          float worldZ) noexcept;

} // namespace SagaEngine::World::Partition
