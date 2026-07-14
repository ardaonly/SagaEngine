/// @file Boundary.cpp
/// @brief Ring classification for the cell apron around a focal cell.

#include "SagaEngine/World/Partition/Boundary.h"

namespace SagaEngine::World::Partition {

// ─── ClassifyCell ──────────────────────────────────────────────────────────

BoundaryRing ClassifyCell(CellCoord             focal,
                          CellCoord             candidate,
                          const BoundaryConfig& config) noexcept
{
    // Chebyshev distance matches the rectangular ring geometry the
    // loader uses: every cell inside radius `r` sits in the
    // `(2r + 1) × (2r + 1)` square centred on the focal cell.
    const std::int32_t cheb = CellChebyshevDistance(focal, candidate);

    if (cheb == 0)
        return BoundaryRing::Focal;

    // Negative radii are treated as "no ring" — the caller can
    // disable the inner apron by setting `innerRadius = 0` and
    // disable the outer apron by matching it to the inner radius.
    const std::int32_t innerRadius = config.innerRadius < 0 ? 0 : config.innerRadius;
    const std::int32_t outerRadius = config.outerRadius < innerRadius
                                         ? innerRadius
                                         : config.outerRadius;

    if (cheb <= innerRadius)
        return BoundaryRing::Inner;

    if (cheb <= outerRadius)
        return BoundaryRing::Outer;

    return BoundaryRing::Outside;
}

// ─── PriorityForRing ───────────────────────────────────────────────────────

SagaEngine::Resources::StreamingPriority PriorityForRing(
    BoundaryRing          ring,
    const BoundaryConfig& config) noexcept
{
    switch (ring)
    {
        case BoundaryRing::Focal:
        case BoundaryRing::Inner:
            return config.innerPriority;

        case BoundaryRing::Outer:
            return config.outerPriority;

        case BoundaryRing::Outside:
        default:
            // Outside cells never generate a streaming request.  The
            // caller should never reach the resource manager for
            // these, but `Low` is the safe fallback if it does.
            return SagaEngine::Resources::StreamingPriority::Low;
    }
}

} // namespace SagaEngine::World::Partition
