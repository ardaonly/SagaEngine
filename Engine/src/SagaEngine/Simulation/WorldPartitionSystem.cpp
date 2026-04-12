/// @file WorldPartitionSystem.cpp
/// @brief World partition system implementation with boundary events and spatial indexing.

#include "SagaEngine/Simulation/WorldPartitionSystem.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Simulation {

static constexpr const char* kTag = "WorldPartitionSystem";

// ─── Configuration ─────────────────────────────────────────────────────────────

void WorldPartitionSystem::Configure(ZoneConfig config) noexcept
{
    config_ = config;
    LOG_INFO(kTag, "World partition configured: zone=%.0f, cells=%ux%u, cellSize=%.0f",
             config.zoneSize, config.cellsPerZoneAxis, config.cellsPerZoneAxis,
             config.zoneSize / static_cast<float>(config.cellsPerZoneAxis));
}

// ─── Entity tracking ──────────────────────────────────────────────────────────

void WorldPartitionSystem::RegisterEntity(ECS::EntityId entityId, const Math::Vec3& position)
{
    if (entityCells_.count(entityId) > 0)
    {
        LOG_WARN(kTag, "Entity %u already registered, use UpdateEntityPosition instead.",
                 entityId);
        return;
    }

    const GlobalCellId cellId = WorldToGlobalCell(position);

    entityCells_[entityId] = cellId;

    PartitionCell& cell = GetOrCreateCell(cellId.zone, cellId.local);
    cell.entities.insert(entityId);

    PartitionZone& zone = GetOrCreateZone(cellId.zone);
    zone.allEntitiesInZone.insert(entityId);

    // Fire enter event
    EntityCellEnterEvent event{entityId, cellId, position};
    if (eventBus_)
    {
        // TODO: Define proper EventId constants
        // eventBus_->Publish<EventId, EntityCellEnterEvent>(event);
    }
}

void WorldPartitionSystem::UnregisterEntity(ECS::EntityId entityId)
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
        return;

    const GlobalCellId cellId = it->second;

    // Remove from cell
    auto zoneIt = zones_.find(cellId.zone);
    if (zoneIt != zones_.end())
    {
        auto cellIt = zoneIt->second.cells.find(cellId.local);
        if (cellIt != zoneIt->second.cells.end())
        {
            cellIt->second.entities.erase(entityId);

            // Fire leave event
            EntityCellLeaveEvent event{entityId, cellId, Math::Vec3::Zero()};
            if (eventBus_)
            {
                // TODO: Define proper EventId constants
                // eventBus_->Publish<EventId, EntityCellLeaveEvent>(event);
            }

            // Clean up empty cells
            if (cellIt->second.entities.empty())
            {
                zoneIt->second.cells.erase(cellIt);
            }
        }

        zoneIt->second.allEntitiesInZone.erase(entityId);

        // Clean up empty zones
        if (zoneIt->second.cells.empty() && zoneIt->second.allEntitiesInZone.empty())
        {
            zones_.erase(zoneIt);
        }
    }

    entityCells_.erase(it);
}

void WorldPartitionSystem::UpdateEntityPosition(ECS::EntityId entityId, const Math::Vec3& position)
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
    {
        LOG_WARN(kTag, "Entity %u not registered, use RegisterEntity first.", entityId);
        return;
    }

    const GlobalCellId oldCellId = it->second;
    const GlobalCellId newCellId = WorldToGlobalCell(position);

    if (oldCellId != newCellId)
    {
        MoveEntityToCell(entityId, oldCellId, newCellId, position);
    }

    it->second = newCellId;
}

std::optional<GlobalCellId> WorldPartitionSystem::GetEntityCell(
    ECS::EntityId entityId) const noexcept
{
    auto it = entityCells_.find(entityId);
    if (it != entityCells_.end())
        return it->second;
    return std::nullopt;
}

std::optional<ZoneId> WorldPartitionSystem::GetEntityZone(
    ECS::EntityId entityId) const noexcept
{
    auto it = entityCells_.find(entityId);
    if (it != entityCells_.end())
        return it->second.zone;
    return std::nullopt;
}

// ─── Spatial queries ──────────────────────────────────────────────────────────

std::vector<ECS::EntityId> WorldPartitionSystem::GetEntitiesInSameCell(
    ECS::EntityId entityId) const
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
        return {};

    const GlobalCellId& cellId = it->second;
    return GetEntitiesInCell(cellId);
}

std::vector<ECS::EntityId> WorldPartitionSystem::GetEntitiesInInterestRange(
    ECS::EntityId entityId) const
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
        return {};

    const GlobalCellId& centerCell = it->second;
    const std::int32_t radius = static_cast<std::int32_t>(config_.interestRadiusCells);

    return CollectEntitiesInCellRange(
        centerCell.zone,
        centerCell.local.localX - radius,
        centerCell.local.localZ - radius,
        centerCell.local.localX + radius,
        centerCell.local.localZ + radius);
}

std::vector<ECS::EntityId> WorldPartitionSystem::GetEntitiesInCell(
    const GlobalCellId& cellId) const
{
    auto zoneIt = zones_.find(cellId.zone);
    if (zoneIt == zones_.end())
        return {};

    auto cellIt = zoneIt->second.cells.find(cellId.local);
    if (cellIt == zoneIt->second.cells.end())
        return {};

    return std::vector<ECS::EntityId>(
        cellIt->second.entities.begin(),
        cellIt->second.entities.end());
}

std::vector<ECS::EntityId> WorldPartitionSystem::GetEntitiesInRadius(
    const Math::Vec3& center, float radius) const
{
    std::vector<ECS::EntityId> result;
    const float radiusSq = radius * radius;

    // Determine which cells overlap with the sphere
    const GlobalCellId centerCell = WorldToGlobalCell(center);
    const float cellSize = CellSize();
    const int cellsRadius = static_cast<int>(std::ceil(radius / cellSize));

    // Iterate over cells in the bounding box of the sphere
    for (int dx = -cellsRadius; dx <= cellsRadius; ++dx)
    {
        for (int dz = -cellsRadius; dz <= cellsRadius; ++dz)
        {
            const std::int16_t testCellX = static_cast<std::int16_t>(
                centerCell.local.localX + dx);
            const std::int16_t testCellZ = static_cast<std::int16_t>(
                centerCell.local.localZ + dz);

            // Clamp to valid zone bounds
            if (testCellX < 0 || testCellX >= static_cast<std::int16_t>(config_.cellsPerZoneAxis) ||
                testCellZ < 0 || testCellZ >= static_cast<std::int16_t>(config_.cellsPerZoneAxis))
            {
                continue;
            }

            GlobalCellId testCellId{centerCell.zone, {testCellX, testCellZ}};
            auto entities = GetEntitiesInCell(testCellId);

            for (const auto& eid : entities)
            {
                // Approximate position from cell (refine if needed with stored positions)
                // For now, include all entities in overlapping cells
                result.push_back(eid);
            }
        }
    }

    // Deduplicate
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

// ─── Zone/cell conversion ─────────────────────────────────────────────────────

ZoneId WorldPartitionSystem::WorldToZone(const Math::Vec3& position) const noexcept
{
    const float zoneSize = config_.zoneSize;
    return ZoneId{
        static_cast<std::int32_t>(std::floor(position.x / zoneSize)),
        static_cast<std::int32_t>(std::floor(position.z / zoneSize))
    };
}

CellCoord WorldPartitionSystem::WorldToCellLocal(const Math::Vec3& position) const noexcept
{
    const float zoneSize = config_.zoneSize;
    const float cellSize = CellSize();

    // Position within the zone
    const float localX = position.x - std::floor(position.x / zoneSize) * zoneSize;
    const float localZ = position.z - std::floor(position.z / zoneSize) * zoneSize;

    return CellCoord{
        static_cast<std::int16_t>(std::min(
            static_cast<std::int32_t>(localX / cellSize),
            static_cast<std::int32_t>(config_.cellsPerZoneAxis) - 1)),
        static_cast<std::int16_t>(std::min(
            static_cast<std::int32_t>(localZ / cellSize),
            static_cast<std::int32_t>(config_.cellsPerZoneAxis) - 1))
    };
}

GlobalCellId WorldPartitionSystem::WorldToGlobalCell(const Math::Vec3& position) const noexcept
{
    return GlobalCellId{WorldToZone(position), WorldToCellLocal(position)};
}

// ─── Zone transition triggers ─────────────────────────────────────────────────

void WorldPartitionSystem::OnZoneEnter(ZoneEnterFn fn)
{
    zoneEnterCallbacks_.push_back(std::move(fn));
}

void WorldPartitionSystem::OnZoneExit(ZoneExitFn fn)
{
    zoneExitCallbacks_.push_back(std::move(fn));
}

// ─── Diagnostics ──────────────────────────────────────────────────────────────

std::size_t WorldPartitionSystem::RegisteredEntityCount() const noexcept
{
    return entityCells_.size();
}

std::size_t WorldPartitionSystem::LoadedZoneCount() const noexcept
{
    return zones_.size();
}

std::size_t WorldPartitionSystem::LoadedCellCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& [_, zone] : zones_)
        count += zone.cells.size();
    return count;
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

PartitionZone& WorldPartitionSystem::GetOrCreateZone(const ZoneId& zoneId)
{
    auto it = zones_.find(zoneId);
    if (it != zones_.end())
        return it->second;

    PartitionZone zone;
    zone.id = zoneId;
    return zones_.emplace(zoneId, std::move(zone)).first->second;
}

PartitionCell& WorldPartitionSystem::GetOrCreateCell(const ZoneId& zoneId,
                                                      const CellCoord& cellCoord)
{
    PartitionZone& zone = GetOrCreateZone(zoneId);

    auto it = zone.cells.find(cellCoord);
    if (it != zone.cells.end())
        return it->second;

    PartitionCell cell;
    return zone.cells.emplace(cellCoord, std::move(cell)).first->second;
}

void WorldPartitionSystem::MoveEntityToCell(ECS::EntityId entityId,
                                             const GlobalCellId& fromCell,
                                             const GlobalCellId& toCell,
                                             const Math::Vec3& position)
{
    // Remove from old cell
    auto fromZoneIt = zones_.find(fromCell.zone);
    if (fromZoneIt != zones_.end())
    {
        auto fromCellIt = fromZoneIt->second.cells.find(fromCell.local);
        if (fromCellIt != fromZoneIt->second.cells.end())
        {
            fromCellIt->second.entities.erase(entityId);

            // Fire leave event
            EntityCellLeaveEvent leaveEvent{entityId, fromCell, position};
            if (eventBus_)
            {
                // TODO: Define proper EventId constants
                // eventBus_->Publish<EventId, EntityCellLeaveEvent>(leaveEvent);
            }

            // Clean up empty cells
            if (fromCellIt->second.entities.empty())
            {
                fromZoneIt->second.cells.erase(fromCellIt);
            }
        }

        fromZoneIt->second.allEntitiesInZone.erase(entityId);

        // Clean up empty zones
        if (fromZoneIt->second.cells.empty() && fromZoneIt->second.allEntitiesInZone.empty())
        {
            // Fire zone exit callbacks
            for (const auto& cb : zoneExitCallbacks_)
                cb(entityId, fromCell.zone);

            zones_.erase(fromZoneIt);
        }
    }

    // Add to new cell
    PartitionCell& toCellRef = GetOrCreateCell(toCell.zone, toCell.local);
    toCellRef.entities.insert(entityId);

    PartitionZone& toZoneRef = GetOrCreateZone(toCell.zone);
    toZoneRef.allEntitiesInZone.insert(entityId);

    // Fire enter event
    EntityCellEnterEvent enterEvent{entityId, toCell, position};
    if (eventBus_)
    {
        // TODO: Define proper EventId constants
        // eventBus_->Publish<EventId, EntityCellEnterEvent>(enterEvent);
    }

    // Fire zone crossing event if zone changed
    if (fromCell.zone != toCell.zone)
    {
        EntityZoneCrossingEvent zoneEvent{entityId, fromCell.zone, toCell.zone, position};
        if (eventBus_)
        {
            // TODO: Define proper EventId constants
            // eventBus_->Publish<EventId, EntityZoneCrossingEvent>(zoneEvent);
        }

        // Fire zone enter callbacks
        for (const auto& cb : zoneEnterCallbacks_)
            cb(entityId, toCell.zone);
    }

    // Fire cell crossing event
    EntityCellCrossingEvent cellEvent{entityId, fromCell, toCell, position};
    if (eventBus_)
    {
        // TODO: Define proper EventId constants
        // eventBus_->Publish<EventId, EntityCellCrossingEvent>(cellEvent);
    }
}

std::vector<ECS::EntityId> WorldPartitionSystem::CollectEntitiesInCellRange(
    const ZoneId& zoneId,
    std::int16_t minCellX, std::int16_t minCellZ,
    std::int16_t maxCellX, std::int16_t maxCellZ) const
{
    std::vector<ECS::EntityId> result;

    auto zoneIt = zones_.find(zoneId);
    if (zoneIt == zones_.end())
        return result;

    // Clamp to valid zone bounds
    minCellX = std::max<std::int16_t>(minCellX, 0);
    minCellZ = std::max<std::int16_t>(minCellZ, 0);
    maxCellX = std::min<std::int16_t>(maxCellX,
                                       static_cast<std::int16_t>(config_.cellsPerZoneAxis) - 1);
    maxCellZ = std::min<std::int16_t>(maxCellZ,
                                       static_cast<std::int16_t>(config_.cellsPerZoneAxis) - 1);

    for (std::int16_t cx = minCellX; cx <= maxCellX; ++cx)
    {
        for (std::int16_t cz = minCellZ; cz <= maxCellZ; ++cz)
        {
            CellCoord coord{cx, cz};
            auto cellIt = zoneIt->second.cells.find(coord);
            if (cellIt != zoneIt->second.cells.end())
            {
                result.insert(result.end(),
                              cellIt->second.entities.begin(),
                              cellIt->second.entities.end());
            }
        }
    }

    // Deduplicate
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

float WorldPartitionSystem::CellSize() const noexcept
{
    return config_.zoneSize / static_cast<float>(config_.cellsPerZoneAxis);
}

} // namespace SagaEngine::Simulation
