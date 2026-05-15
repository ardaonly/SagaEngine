/// @file Cell.cpp
/// @brief Coordinate helpers for the world partition cell grid.

#include "SagaEngine/World/Partition/Cell.h"

#include <cmath>

namespace SagaEngine::World::Partition {

// ─── CellCoordFromWorld ────────────────────────────────────────────────────

CellCoord CellCoordFromWorld(float worldX,
                             float worldZ,
                             float cellSizeMetres) noexcept
{
    if (cellSizeMetres <= 0.0f)
        return CellCoord{0, 0};

    // `floor` semantics are mandatory: a truncation towards zero would
    // split the grid on the sign boundary and produce two cells at
    // (0, 0), which breaks every hash-based lookup.
    const float fx = std::floor(worldX / cellSizeMetres);
    const float fz = std::floor(worldZ / cellSizeMetres);
    return CellCoord{
        static_cast<std::int32_t>(fx),
        static_cast<std::int32_t>(fz),
    };
}

// ─── FootprintFromCoord ────────────────────────────────────────────────────

CellFootprint FootprintFromCoord(CellCoord coord,
                                 float     cellSizeMetres,
                                 float     minY,
                                 float     maxY) noexcept
{
    CellFootprint fp;
    fp.minX = static_cast<float>(coord.x) * cellSizeMetres;
    fp.minZ = static_cast<float>(coord.z) * cellSizeMetres;
    fp.maxX = fp.minX + cellSizeMetres;
    fp.maxZ = fp.minZ + cellSizeMetres;
    fp.minY = minY;
    fp.maxY = maxY;
    return fp;
}

// ─── DistanceSqToFootprint ─────────────────────────────────────────────────

float DistanceSqToFootprint(const CellFootprint& footprint,
                            float worldX,
                            float worldZ) noexcept
{
    // Clamp the query point to the footprint and return the squared
    // distance from the point to the clamped position.  This is the
    // standard 2D AABB point-distance formula and returns zero for
    // points that lie inside the footprint.
    float dx = 0.0f;
    float dz = 0.0f;

    if (worldX < footprint.minX)
        dx = footprint.minX - worldX;
    else if (worldX > footprint.maxX)
        dx = worldX - footprint.maxX;

    if (worldZ < footprint.minZ)
        dz = footprint.minZ - worldZ;
    else if (worldZ > footprint.maxZ)
        dz = worldZ - footprint.maxZ;

    return dx * dx + dz * dz;
}

} // namespace SagaEngine::World::Partition
