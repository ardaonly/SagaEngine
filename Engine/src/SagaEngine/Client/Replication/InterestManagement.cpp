/// @file InterestManagement.cpp
/// @brief Zone-partitioned relevancy system for MMO-scale replication.

#include "SagaEngine/Client/Replication/InterestManagement.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cmath>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void InterestManager::Configure(InterestConfig config) noexcept
{
    config_ = config;
}

// ─── World to cell ──────────────────────────────────────────────────────────

CellCoord InterestManager::WorldToCell(const Math::Vec3& position) const noexcept
{
    std::int32_t cx = static_cast<std::int32_t>(std::floor(position.x / config_.cellSize));
    std::int32_t cy = static_cast<std::int32_t>(std::floor(position.y / config_.cellSize));
    return {cx, cy};
}

// ─── Update client position ─────────────────────────────────────────────────

void InterestManager::UpdateClientPosition(const Math::Vec3& position) noexcept
{
    CellCoord newCell = WorldToCell(position);

    if (newCell.x == clientCell_.x && newCell.y == clientCell_.y)
        return;  // No change — skip recomputation.

    clientCell_ = newCell;

    // Recompute subscribed cells with hysteresis.
    std::int32_t effectiveRadius = static_cast<std::int32_t>(
        config_.interestRadius + config_.hysteresisMargin);

    subscribedCells_.clear();

    for (std::int32_t dx = -effectiveRadius; dx <= effectiveRadius; ++dx)
    {
        for (std::int32_t dy = -effectiveRadius; dy <= effectiveRadius; ++dy)
        {
            CellCoord cell{clientCell_.x + dx, clientCell_.y + dy};

            // Check if within strict interest radius (Manhattan distance).
            std::int32_t manhattan = std::abs(dx) + std::abs(dy);
            if (manhattan <= static_cast<std::int32_t>(config_.interestRadius + config_.hysteresisMargin))
                subscribedCells_.insert(cell);
        }
    }
}

// ─── Entity registration ───────────────────────────────────────────────────

void InterestManager::RegisterEntity(ECS::EntityId entityId, const Math::Vec3& position) noexcept
{
    CellCoord cell = WorldToCell(position);
    entityCells_[entityId] = cell;
    cellEntities_[cell].insert(entityId);
}

void InterestManager::UnregisterEntity(ECS::EntityId entityId) noexcept
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
        return;

    CellCoord cell = it->second;
    entityCells_.erase(it);

    auto cellIt = cellEntities_.find(cell);
    if (cellIt != cellEntities_.end())
    {
        cellIt->second.erase(entityId);
        if (cellIt->second.empty())
            cellEntities_.erase(cellIt);
    }
}

void InterestManager::UpdateEntityPosition(ECS::EntityId entityId, const Math::Vec3& position) noexcept
{
    auto it = entityCells_.find(entityId);
    CellCoord oldCell = (it != entityCells_.end()) ? it->second : CellCoord{0, 0};
    CellCoord newCell = WorldToCell(position);

    if (oldCell.x == newCell.x && oldCell.y == newCell.y)
    {
        // Cell unchanged — just update position mapping.
        entityCells_[entityId] = newCell;
        return;
    }

    // Remove from old cell.
    if (it != entityCells_.end())
    {
        auto cellIt = cellEntities_.find(oldCell);
        if (cellIt != cellEntities_.end())
        {
            cellIt->second.erase(entityId);
            if (cellIt->second.empty())
                cellEntities_.erase(cellIt);
        }
    }

    // Add to new cell.
    entityCells_[entityId] = newCell;
    cellEntities_[newCell].insert(entityId);
}

// ─── Relevancy check ────────────────────────────────────────────────────────

bool InterestManager::IsRelevant(ECS::EntityId entityId) const noexcept
{
    auto it = entityCells_.find(entityId);
    if (it == entityCells_.end())
        return false;

    return subscribedCells_.count(it->second) > 0;
}

std::unordered_set<ECS::EntityId> InterestManager::GetRelevantEntities() const noexcept
{
    std::unordered_set<ECS::EntityId> result;

    for (const auto& cell : subscribedCells_)
    {
        auto it = cellEntities_.find(cell);
        if (it != cellEntities_.end())
        {
            for (ECS::EntityId id : it->second)
                result.insert(id);
        }
    }

    return result;
}

std::size_t InterestManager::RelevantCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& cell : subscribedCells_)
    {
        auto it = cellEntities_.find(cell);
        if (it != cellEntities_.end())
            count += it->second.size();
    }
    return count;
}

} // namespace SagaEngine::Client::Replication
