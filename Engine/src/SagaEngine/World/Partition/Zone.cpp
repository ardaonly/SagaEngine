/// @file Zone.cpp
/// @brief Zone grid layout and cell lookup implementation.

#include "SagaEngine/World/Partition/Zone.h"

#include "SagaEngine/Core/Log/Log.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace SagaEngine::World::Partition {

namespace {

constexpr const char* kLogTag = "World";

} // namespace

// ─── Configure ─────────────────────────────────────────────────────────────

void Zone::Configure(ZoneId             zoneId,
                     const std::string& debugName,
                     ZoneGridConfig     grid) noexcept
{
    zoneId_    = zoneId;
    debugName_ = debugName;
    grid_      = grid;

    // Guard against pathological input.  A zero-sized grid is
    // legal (some test fixtures want an empty zone) but a negative
    // count would underflow the size_t multiplication below.
    if (grid_.cellCountX < 0)
        grid_.cellCountX = 0;
    if (grid_.cellCountZ < 0)
        grid_.cellCountZ = 0;

    const auto total =
        static_cast<std::size_t>(grid_.cellCountX) *
        static_cast<std::size_t>(grid_.cellCountZ);

    cells_.clear();
    cells_.resize(total);

    // Populate each cell's coord + footprint eagerly so the loader
    // does not have to recompute them on every distance test.
    for (std::int32_t z = 0; z < grid_.cellCountZ; ++z)
    {
        for (std::int32_t x = 0; x < grid_.cellCountX; ++x)
        {
            const std::size_t idx =
                static_cast<std::size_t>(z) *
                static_cast<std::size_t>(grid_.cellCountX) +
                static_cast<std::size_t>(x);

            Cell& cell = cells_[idx];
            cell.coord = CellCoord{x, z};

            CellFootprint fp;
            fp.minX = grid_.originX + static_cast<float>(x) * grid_.cellSizeMetres;
            fp.minZ = grid_.originZ + static_cast<float>(z) * grid_.cellSizeMetres;
            fp.maxX = fp.minX + grid_.cellSizeMetres;
            fp.maxZ = fp.minZ + grid_.cellSizeMetres;
            fp.minY = -256.0f;
            fp.maxY =  1024.0f;
            cell.footprint = fp;
            cell.state     = CellState::Dormant;
        }
    }

    LOG_INFO(kLogTag,
             "Zone::Configure id=%u name='%s' cells=%dx%d cellSize=%.1fm",
             static_cast<unsigned>(zoneId_),
             debugName_.c_str(),
             grid_.cellCountX,
             grid_.cellCountZ,
             static_cast<double>(grid_.cellSizeMetres));
}

// ─── IndexFromCoord ────────────────────────────────────────────────────────

std::size_t Zone::IndexFromCoord(CellCoord coord) const noexcept
{
    if (coord.x < 0 || coord.z < 0)
        return std::numeric_limits<std::size_t>::max();
    if (coord.x >= grid_.cellCountX || coord.z >= grid_.cellCountZ)
        return std::numeric_limits<std::size_t>::max();

    return static_cast<std::size_t>(coord.z) *
           static_cast<std::size_t>(grid_.cellCountX) +
           static_cast<std::size_t>(coord.x);
}

// ─── FindCell ──────────────────────────────────────────────────────────────

Cell* Zone::FindCell(CellCoord coord) noexcept
{
    const std::size_t idx = IndexFromCoord(coord);
    if (idx >= cells_.size())
        return nullptr;
    return &cells_[idx];
}

const Cell* Zone::FindCell(CellCoord coord) const noexcept
{
    const std::size_t idx = IndexFromCoord(coord);
    if (idx >= cells_.size())
        return nullptr;
    return &cells_[idx];
}

// ─── WorldToCell ───────────────────────────────────────────────────────────

CellCoord Zone::WorldToCell(float worldX, float worldZ) const noexcept
{
    if (grid_.cellSizeMetres <= 0.0f)
        return CellCoord{0, 0};

    const float localX = worldX - grid_.originX;
    const float localZ = worldZ - grid_.originZ;

    return CellCoord{
        static_cast<std::int32_t>(std::floor(localX / grid_.cellSizeMetres)),
        static_cast<std::int32_t>(std::floor(localZ / grid_.cellSizeMetres)),
    };
}

// ─── ContainsCoord ─────────────────────────────────────────────────────────

bool Zone::ContainsCoord(CellCoord coord) const noexcept
{
    return coord.x >= 0 &&
           coord.z >= 0 &&
           coord.x < grid_.cellCountX &&
           coord.z < grid_.cellCountZ;
}

} // namespace SagaEngine::World::Partition
