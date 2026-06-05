/// @file Boundary.h
/// @brief Apron descriptor — the interior / exterior ring of a cell.
///
/// Layer  : SagaEngine / World / Partition
/// Purpose: The zone loader never streams a single cell in isolation.
///          When the player moves towards a new cell the loader must
///          also warm the cells immediately around the destination so
///          the player never sees a blank tile crossing the boundary.
///          `Boundary` is the descriptor that captures which cells
///          form that warm ring and how aggressive the preload should
///          be.
///
///          A boundary has two concentric rings:
///
///            - the *inner* apron contains cells whose assets must be
///              fully resident before the player is allowed to walk
///              into the centre cell; any streaming failure here
///              promotes the cell from `Prefetching` back to `Dormant`
///              so the loader retries on the next tick.
///            - the *outer* apron contains speculative cells that the
///              loader prefetches at `Low` priority; these may silently
///              fail if the streaming budget is tight and the player
///              is not actually going to cross into them.
///
///          The cross-zone handshake (see `CrossZoneVisibility.h`)
///          takes over once the player is standing on a seam between
///          two zones.  Inside a single zone the boundary is a local
///          concept that never crosses a process boundary.
///
/// Design rules:
///   - Radius-based.  The apron is described as a pair of radii in
///     cell units rather than as an explicit list of coordinates;
///     this keeps the descriptor small and lets the loader resolve
///     the actual cells on the fly via a cheap ring iteration.
///   - Priority-weighted.  Each ring carries its own streaming
///     priority so the loader can submit all inner-ring cells at
///     `High` and all outer-ring cells at `Low` with a single pass.
///   - Zone-local.  Boundaries never reach across zones; cross-zone
///     streaming is handled by the apron replicas in
///     `CrossZoneVisibility.h`.
///
/// What this header is NOT:
///   - Not a visibility query.  Whether a cell is *rendered* is a
///     per-camera culling decision; the apron is strictly about
///     *residency*, not visibility.
///   - Not a pathfinding primitive.  Navmesh and A* live in the
///     simulation layer and do not look at boundaries.

#pragma once

#include "SagaEngine/Resources/StreamingRequest.h"
#include "SagaEngine/World/Partition/Cell.h"

#include <cstdint>

namespace SagaEngine::World::Partition {

// ─── Boundary configuration ────────────────────────────────────────────────

/// Shape of the apron around a focal cell.  The focal cell is always
/// treated as `innerRadius = 0`; the caller configures how many rings
/// of neighbours should be forcibly resident and how many should be
/// speculatively warmed.
struct BoundaryConfig
{
    /// Inner apron radius in cell units.  A value of `1` means "the
    /// eight immediate neighbours"; a value of `2` means "the 24
    /// cells in the 5×5 block".  Zero disables the inner apron and
    /// makes the boundary collapse to the focal cell only.
    std::int32_t innerRadius = 1;

    /// Outer apron radius in cell units.  Must be greater than or
    /// equal to `innerRadius`; the outer ring is the set of cells
    /// in the square of radius `outerRadius` minus the cells in
    /// the square of radius `innerRadius`.
    std::int32_t outerRadius = 2;

    /// Priority submitted to the streaming manager for inner-ring
    /// cells.  `High` is the default because the player may walk
    /// into these cells within a handful of frames.
    SagaEngine::Resources::StreamingPriority innerPriority =
        SagaEngine::Resources::StreamingPriority::High;

    /// Priority submitted to the streaming manager for outer-ring
    /// cells.  `Low` is the default; these cells are opportunistic
    /// and may be dropped if the budget is tight.
    SagaEngine::Resources::StreamingPriority outerPriority =
        SagaEngine::Resources::StreamingPriority::Low;

    /// Number of ticks a cell must remain outside both rings before
    /// its assets are eligible for eviction.  A non-zero drain
    /// grace period prevents the loader from thrashing when the
    /// player walks along a boundary.
    std::uint32_t drainGraceTicks = 64;
};

// ─── Boundary helpers ──────────────────────────────────────────────────────

/// Classification of a cell relative to a boundary.  Returned by
/// `ClassifyCell` so the loader can drive the state machine with
/// a single switch statement.
enum class BoundaryRing : std::uint8_t
{
    /// Cell is strictly outside both rings.  The loader must drain it.
    Outside = 0,

    /// Cell is inside the outer apron but outside the inner apron.
    /// Prefetch at outer priority; tolerate streaming failure.
    Outer,

    /// Cell is inside the inner apron.  Prefetch at inner priority;
    /// a streaming failure here re-queues the request.
    Inner,

    /// Cell is the focal cell itself.  Always loaded.
    Focal,
};

/// Classify a cell coordinate relative to a focal coordinate and a
/// boundary configuration.  Pure function; does not touch any state.
[[nodiscard]] BoundaryRing ClassifyCell(CellCoord            focal,
                                        CellCoord            candidate,
                                        const BoundaryConfig& config) noexcept;

/// Chebyshev distance between two cell coordinates — the side length
/// of the smallest square ring centred on `a` that touches `b`.  This
/// is the metric the boundary rings use (as opposed to Euclidean
/// distance, which would produce a disc and require sqrt).
[[nodiscard]] constexpr std::int32_t CellChebyshevDistance(CellCoord a,
                                                           CellCoord b) noexcept
{
    const std::int32_t dx = a.x - b.x;
    const std::int32_t dz = a.z - b.z;
    const std::int32_t ax = dx < 0 ? -dx : dx;
    const std::int32_t az = dz < 0 ? -dz : dz;
    return ax > az ? ax : az;
}

/// Return the streaming priority associated with a given ring.
/// Centralised here so the loader and the diagnostics panel agree
/// on the mapping.
[[nodiscard]] SagaEngine::Resources::StreamingPriority PriorityForRing(
    BoundaryRing          ring,
    const BoundaryConfig& config) noexcept;

} // namespace SagaEngine::World::Partition
