/// @file Zone.h
/// @brief Collection of cells that make up one streamable zone.
///
/// Layer  : SagaEngine / World / Partition
/// Purpose: A `Zone` is the coarse-grained spatial container that owns
///          a rectangular patch of the world.  One zone maps one-to-one
///          to one authoritative server process (see `ZoneServer`),
///          so the boundary of a zone is also the boundary between
///          shards.  Inside a zone the world is tiled by a regular
///          grid of `Cell` descriptors, and the streaming subsystem
///          operates on the cells rather than the whole zone — a
///          single zone is far too large to ever be fully resident.
///
///          This header is deliberately pure data: it defines how a
///          zone is laid out and how to look cells up inside it, but
///          it does not perform any streaming or ECS work.  That is
///          the responsibility of `World::Streaming::ResourceStream`
///          (which consumes zones and drives the loader) and of
///          `ZoneServer` (which ties a zone to its tick loop).
///
/// Design rules:
///   - Fixed grid.  Every cell in a zone has the same size; irregular
///     partitioning is out of scope because the streaming loader
///     relies on integer grid coordinates for stable hashing and
///     cross-process agreement.
///   - Stable ids.  The zone id is a 32-bit unsigned integer assigned
///     by the content tooling and is persistent across cooks; the
///     shard mesh (see `ZoneTransferProtocol.h`) uses it to address
///     zones across processes.
///   - Cells are owned.  A zone owns a contiguous vector of `Cell`
///     descriptors; the loader borrows references but never mutates
///     the registry copy.
///
/// What this header is NOT:
///   - Not a simulation container.  The ECS worlds live in
///     `Simulation::WorldState`; zones contribute their cell list
///     to the streaming subsystem, not to the tick loop.
///   - Not a server descriptor.  The tick rate, authority model, and
///     network config live in `ZoneServerConfig`; this header only
///     cares about spatial layout.

#pragma once

#include "SagaEngine/World/Partition/Cell.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::World::Partition {

// ─── Zone id ───────────────────────────────────────────────────────────────

/// Stable identifier for a zone.  Assigned at content-cook time and
/// carried through the shard mesh so every process agrees on which
/// zone is which.
using ZoneId = std::uint32_t;

/// Reserved value meaning "no zone".  Used as a sentinel in lookup
/// tables and in cross-zone handshake messages.
inline constexpr ZoneId kInvalidZoneId = 0;

// ─── Zone configuration ────────────────────────────────────────────────────

/// Grid layout configuration for a zone.  Created by the content
/// tools and loaded alongside the cell registry.
struct ZoneGridConfig
{
    /// Side length of one cell in world-space metres.  A smaller
    /// value gives finer streaming granularity at the cost of more
    /// descriptor rows; a larger value reduces overhead but wastes
    /// memory on assets the player cannot see yet.  64–128 metres
    /// is the sweet spot for open-world terrain.
    float cellSizeMetres = 128.0f;

    /// Number of cells along the X axis.  Cells are addressed from
    /// `0` to `cellCountX - 1` in grid-space.
    std::int32_t cellCountX = 64;

    /// Number of cells along the Z axis.
    std::int32_t cellCountZ = 64;

    /// World-space offset of the `(0, 0)` cell's minimum corner.
    /// Zones that do not sit at the world origin carry a non-zero
    /// offset so the grid remains rectangular.
    float originX = 0.0f;
    float originZ = 0.0f;
};

// ─── Zone descriptor ───────────────────────────────────────────────────────

/// Complete description of one zone.  Owns the cell list and knows
/// how to look cells up by integer grid coordinate.
class Zone
{
public:
    Zone() noexcept  = default;
    ~Zone() noexcept = default;

    Zone(const Zone&)            = delete;
    Zone& operator=(const Zone&) = delete;
    Zone(Zone&&) noexcept        = default;
    Zone& operator=(Zone&&) noexcept = default;

    // ─── Construction ────────────────────────────────────────────────────

    /// Configure the zone with a fresh grid.  Any previously stored
    /// cells are dropped; the new grid is allocated with the right
    /// number of cells, each initialised to `Dormant`.
    void Configure(ZoneId zoneId, const std::string& debugName, ZoneGridConfig grid) noexcept;

    // ─── Identity ────────────────────────────────────────────────────────

    [[nodiscard]] ZoneId             Id()       const noexcept { return zoneId_; }
    [[nodiscard]] const std::string& DebugName() const noexcept { return debugName_; }
    [[nodiscard]] const ZoneGridConfig& Grid()  const noexcept { return grid_; }

    // ─── Cell access ─────────────────────────────────────────────────────

    /// Number of cells in the zone (always `cellCountX * cellCountZ`
    /// once `Configure` has been called).
    [[nodiscard]] std::size_t CellCount() const noexcept { return cells_.size(); }

    /// Look up a cell by integer grid coordinate.  Returns nullptr
    /// if the coordinate is out of range.
    [[nodiscard]] Cell*       FindCell(CellCoord coord) noexcept;
    [[nodiscard]] const Cell* FindCell(CellCoord coord) const noexcept;

    /// Mutable access for the registry loader.  Returns a span-like
    /// reference so the caller can populate `assetIds` without
    /// copying the whole vector.
    [[nodiscard]] std::vector<Cell>&       Cells()       noexcept { return cells_; }
    [[nodiscard]] const std::vector<Cell>& Cells() const noexcept { return cells_; }

    // ─── Coordinate helpers ──────────────────────────────────────────────

    /// Convert a world-space XZ position to the cell coordinate
    /// that contains it, accounting for the zone's origin offset.
    /// The result may lie outside the grid bounds if the position
    /// is not inside the zone; callers should verify with
    /// `ContainsCoord` before dereferencing.
    [[nodiscard]] CellCoord WorldToCell(float worldX, float worldZ) const noexcept;

    /// Test whether a grid coordinate is inside the zone's cell
    /// range.  Out-of-range coordinates identify cells that live
    /// in a neighbouring zone.
    [[nodiscard]] bool ContainsCoord(CellCoord coord) const noexcept;

private:
    /// Flatten a 2D grid coordinate to a row-major index into
    /// `cells_`.  Returns `SIZE_MAX` if the coordinate is out of
    /// range so callers can detect failure without a second check.
    [[nodiscard]] std::size_t IndexFromCoord(CellCoord coord) const noexcept;

    ZoneId            zoneId_ = kInvalidZoneId;
    std::string       debugName_;
    ZoneGridConfig    grid_;
    std::vector<Cell> cells_;
};

} // namespace SagaEngine::World::Partition
