/// @file VisibilityGraph.cpp
/// @brief VisibilityGraph implementation — uniform grid spatial partitioning.

#include "SagaServer/Networking/Interest/VisibilityGraph.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <mutex>
#include <utility>

namespace SagaEngine::Networking::Interest
{

static constexpr const char* kTag = "VisibilityGraph";

// ─── Construction ─────────────────────────────────────────────────────────────

VisibilityGraph::VisibilityGraph(VisibilityGraphConfig config)
    : m_Config(config)
{
    if (m_Config.cellSize <= 0.0f)
    {
        LOG_WARN(kTag, "cellSize %.3f invalid — clamping to 1.0", m_Config.cellSize);
        m_Config.cellSize = 1.0f;
    }

    if (m_Config.defaultViewRadius < 0)
        m_Config.defaultViewRadius = 0;

    if (m_Config.maxViewRadius < m_Config.defaultViewRadius)
        m_Config.maxViewRadius = m_Config.defaultViewRadius;

    m_Cells.reserve(m_Config.reserveCells);
}

// ─── Cell Lookup Helpers ──────────────────────────────────────────────────────

CellCoord VisibilityGraph::CellFromPosition(const Vector3& position) const noexcept
{
    const float invCell = 1.0f / m_Config.cellSize;
    return CellCoord{
        static_cast<int32_t>(std::floor(position.x * invCell)),
        static_cast<int32_t>(std::floor(position.z * invCell))
    };
}

VisibilityCell& VisibilityGraph::TouchCell(const CellCoord& coord)
{
    auto it = m_Cells.find(coord);
    if (it == m_Cells.end())
    {
        VisibilityCell cell;
        cell.coord          = coord;
        cell.lastUpdateTick = m_CurrentTick;
        it = m_Cells.emplace(coord, std::move(cell)).first;
    }
    return it->second;
}

// ─── Entity Lifecycle ─────────────────────────────────────────────────────────

void VisibilityGraph::UpdateEntity(EntityId entity, const Vector3& position)
{
    const CellCoord target = CellFromPosition(position);

    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto indexIt = m_EntityIndex.find(entity);

    if (indexIt != m_EntityIndex.end())
    {
        if (indexIt->second == target)
            return;

        auto cellIt = m_Cells.find(indexIt->second);
        if (cellIt != m_Cells.end())
        {
            cellIt->second.entities.erase(entity);
            cellIt->second.lastUpdateTick = m_CurrentTick;
        }
        indexIt->second = target;
    }
    else
    {
        m_EntityIndex.emplace(entity, target);
    }

    VisibilityCell& cell = TouchCell(target);
    cell.entities.insert(entity);
    cell.lastUpdateTick = m_CurrentTick;
}

void VisibilityGraph::RemoveEntity(EntityId entity)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    DetachEntityLocked(entity);
}

void VisibilityGraph::DetachEntityLocked(EntityId entity)
{
    auto indexIt = m_EntityIndex.find(entity);
    if (indexIt == m_EntityIndex.end())
        return;

    auto cellIt = m_Cells.find(indexIt->second);
    if (cellIt != m_Cells.end())
    {
        cellIt->second.entities.erase(entity);
        cellIt->second.lastUpdateTick = m_CurrentTick;
    }

    m_EntityIndex.erase(indexIt);
}

// ─── Client Lifecycle ─────────────────────────────────────────────────────────

void VisibilityGraph::UpdateClient(ClientId client, const Vector3& position, int32_t viewRadius)
{
    if (viewRadius < 0)
        viewRadius = m_Config.defaultViewRadius;

    if (viewRadius > m_Config.maxViewRadius)
        viewRadius = m_Config.maxViewRadius;

    const CellCoord centre = CellFromPosition(position);

    std::unordered_set<CellCoord, CellCoordHash> nextSet;
    const std::size_t span = static_cast<std::size_t>(2 * viewRadius + 1);
    nextSet.reserve(span * span);

    for (int32_t dz = -viewRadius; dz <= viewRadius; ++dz)
    {
        for (int32_t dx = -viewRadius; dx <= viewRadius; ++dx)
        {
            nextSet.insert(CellCoord{centre.x + dx, centre.z + dz});
        }
    }

    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto& subs = m_ClientSubscriptions[client];
    const auto previous = subs;

    for (const auto& coord : previous)
    {
        if (nextSet.find(coord) == nextSet.end())
        {
            auto it = m_Cells.find(coord);
            if (it != m_Cells.end())
            {
                it->second.subscribers.erase(client);
                it->second.lastUpdateTick = m_CurrentTick;
            }
        }
    }

    for (const auto& coord : nextSet)
    {
        if (previous.find(coord) == previous.end())
        {
            VisibilityCell& cell = TouchCell(coord);
            cell.subscribers.insert(client);
            cell.lastUpdateTick = m_CurrentTick;
        }
    }

    subs = nextSet;

    if (m_OnEntityVisible || m_OnEntityInvisible)
        DispatchTransitionsLocked(client, previous, nextSet);
}

void VisibilityGraph::RemoveClient(ClientId client)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    auto it = m_ClientSubscriptions.find(client);
    if (it == m_ClientSubscriptions.end())
        return;

    for (const auto& coord : it->second)
    {
        auto cellIt = m_Cells.find(coord);
        if (cellIt != m_Cells.end())
        {
            cellIt->second.subscribers.erase(client);
            cellIt->second.lastUpdateTick = m_CurrentTick;
        }
    }

    m_ClientSubscriptions.erase(it);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

std::vector<EntityId> VisibilityGraph::QueryVisibleEntities(ClientId client) const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    return CollectVisibleLocked(client);
}

std::vector<EntityId> VisibilityGraph::CollectVisibleLocked(ClientId client) const
{
    std::vector<EntityId> out;

    auto it = m_ClientSubscriptions.find(client);
    if (it == m_ClientSubscriptions.end() || it->second.empty())
        return out;

    std::unordered_set<EntityId> seen;
    seen.reserve(it->second.size() * 8);

    for (const auto& coord : it->second)
    {
        auto cellIt = m_Cells.find(coord);
        if (cellIt == m_Cells.end())
            continue;

        for (EntityId e : cellIt->second.entities)
        {
            if (seen.insert(e).second)
                out.push_back(e);
        }
    }

    return out;
}

std::vector<EntityId> VisibilityGraph::QueryEntitiesInCell(const Vector3& position) const
{
    const CellCoord coord = CellFromPosition(position);

    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    std::vector<EntityId> out;
    auto it = m_Cells.find(coord);
    if (it == m_Cells.end())
        return out;

    out.reserve(it->second.entities.size());
    for (EntityId e : it->second.entities)
        out.push_back(e);

    return out;
}

std::vector<EntityId> VisibilityGraph::QueryEntitiesInRadius(const Vector3& position,
                                                              int32_t        viewRadius) const
{
    if (viewRadius < 0)
        viewRadius = m_Config.defaultViewRadius;
    if (viewRadius > m_Config.maxViewRadius)
        viewRadius = m_Config.maxViewRadius;

    const CellCoord centre = CellFromPosition(position);

    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    std::vector<EntityId> out;
    std::unordered_set<EntityId> seen;

    for (int32_t dz = -viewRadius; dz <= viewRadius; ++dz)
    {
        for (int32_t dx = -viewRadius; dx <= viewRadius; ++dx)
        {
            const CellCoord probe{centre.x + dx, centre.z + dz};
            auto it = m_Cells.find(probe);
            if (it == m_Cells.end())
                continue;

            for (EntityId e : it->second.entities)
            {
                if (seen.insert(e).second)
                    out.push_back(e);
            }
        }
    }

    return out;
}

std::vector<ClientId> VisibilityGraph::QueryObserversOf(EntityId entity) const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    std::vector<ClientId> out;

    auto indexIt = m_EntityIndex.find(entity);
    if (indexIt == m_EntityIndex.end())
        return out;

    auto cellIt = m_Cells.find(indexIt->second);
    if (cellIt == m_Cells.end())
        return out;

    out.reserve(cellIt->second.subscribers.size());
    for (ClientId c : cellIt->second.subscribers)
        out.push_back(c);

    return out;
}

// ─── Bookkeeping ──────────────────────────────────────────────────────────────

void VisibilityGraph::Clear()
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    m_Cells.clear();
    m_EntityIndex.clear();
    m_ClientSubscriptions.clear();

    LOG_INFO(kTag, "VisibilityGraph cleared");
}

std::size_t VisibilityGraph::PruneEmptyCells()
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);

    std::size_t pruned = 0;

    for (auto it = m_Cells.begin(); it != m_Cells.end(); /* advance in body */)
    {
        if (it->second.IsEmpty())
        {
            it = m_Cells.erase(it);
            ++pruned;
        }
        else
        {
            ++it;
        }
    }

    if (pruned > 0)
        LOG_DEBUG(kTag, "Pruned %zu empty cells (remaining %zu)", pruned, m_Cells.size());

    return pruned;
}

void VisibilityGraph::Tick(std::uint64_t serverTick)
{
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_CurrentTick = serverTick;
}

// ─── Transition Dispatch ──────────────────────────────────────────────────────

void VisibilityGraph::DispatchTransitionsLocked(
    ClientId                                             client,
    const std::unordered_set<CellCoord, CellCoordHash>&  previous,
    const std::unordered_set<CellCoord, CellCoordHash>&  next)
{
    if (m_OnEntityInvisible)
    {
        for (const auto& coord : previous)
        {
            if (next.find(coord) != next.end())
                continue;

            auto cellIt = m_Cells.find(coord);
            if (cellIt == m_Cells.end())
                continue;

            for (EntityId e : cellIt->second.entities)
                m_OnEntityInvisible(client, e);
        }
    }

    if (m_OnEntityVisible)
    {
        for (const auto& coord : next)
        {
            if (previous.find(coord) != previous.end())
                continue;

            auto cellIt = m_Cells.find(coord);
            if (cellIt == m_Cells.end())
                continue;

            for (EntityId e : cellIt->second.entities)
                m_OnEntityVisible(client, e);
        }
    }
}

// ─── Introspection ────────────────────────────────────────────────────────────

VisibilityGraphStats VisibilityGraph::GetStatistics() const
{
    std::shared_lock<std::shared_mutex> lock(m_Mutex);

    VisibilityGraphStats stats;
    stats.totalCells       = m_Cells.size();
    stats.trackedEntities  = m_EntityIndex.size();
    stats.trackedClients   = m_ClientSubscriptions.size();
    stats.lastRebuildTick  = m_CurrentTick;

    std::size_t totalSubs = 0;
    for (const auto& [_, cells] : m_ClientSubscriptions)
        totalSubs += cells.size();
    stats.totalSubscriptions = totalSubs;

    return stats;
}

} // namespace SagaEngine::Networking::Interest
