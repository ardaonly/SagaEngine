/// @file ResourceStream.cpp
/// @brief World-level streaming orchestrator implementation.

#include "SagaEngine/World/Streaming/ResourceStream.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <utility>

namespace SagaEngine::World::Streaming {

namespace {

constexpr const char* kLogTag = "World";

/// Helper: map a `CellState` to a short log-friendly token.  Used
/// only when `logStateTransitions` is enabled so the diagnostics
/// overlay can grep the log for a particular cell's history.
[[nodiscard]] const char* CellStateName(
    SagaEngine::World::Partition::CellState state) noexcept
{
    using SagaEngine::World::Partition::CellState;
    switch (state)
    {
        case CellState::Dormant:     return "Dormant";
        case CellState::Prefetching: return "Prefetching";
        case CellState::Loaded:      return "Loaded";
        case CellState::Draining:    return "Draining";
    }
    return "?";
}

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

ResourceStream::ResourceStream(
    SagaEngine::World::Partition::Zone&     zone,
    SagaEngine::Resources::ResourceManager& resourceManager,
    SagaEngine::Resources::StreamingBudget& budget,
    ResourceStreamConfig                    config) noexcept
    : zone_(zone)
    , resourceManager_(resourceManager)
    , budget_(budget)
    , config_(std::move(config))
{
    lodManager_.Configure(config_.lod);

    LOG_INFO(kLogTag,
             "ResourceStream: bound to zone id=%u cells=%zu inner=%d outer=%d",
             static_cast<unsigned>(zone_.Id()),
             zone_.CellCount(),
             config_.boundary.innerRadius,
             config_.boundary.outerRadius);
}

// ─── Destruction ───────────────────────────────────────────────────────────

ResourceStream::~ResourceStream() noexcept
{
    // Releasing the map drops every `ResidencyHandle` it contains,
    // which in turn decrements the refcount on every asset we were
    // keeping alive.  The `ResourceManager` takes care of the
    // eviction bookkeeping from there.
    trackedCells_.clear();
}

// ─── Tick ──────────────────────────────────────────────────────────────────

ResourceStreamTickStats ResourceStream::Tick(float         focalWorldX,
                                             float         focalWorldZ,
                                             std::uint64_t currentTick)
{
    lastTickStats_                = ResourceStreamTickStats{};
    lastTickStats_.budgetPressure = CurrentPressure();

    const auto focalCoord = zone_.WorldToCell(focalWorldX, focalWorldZ);

    // Walk every cell inside the outer radius square centred on the
    // focal coord.  For small radii (1..4) this is a handful of
    // cells and a plain nested loop beats a spatial index.
    const std::int32_t r = config_.boundary.outerRadius < 0 ? 0 : config_.boundary.outerRadius;

    for (std::int32_t dz = -r; dz <= r; ++dz)
    {
        for (std::int32_t dx = -r; dx <= r; ++dx)
        {
            SagaEngine::World::Partition::CellCoord candidate{
                focalCoord.x + dx,
                focalCoord.z + dz,
            };

            if (!zone_.ContainsCoord(candidate))
                continue;

            const SagaEngine::World::Partition::Cell* cell = zone_.FindCell(candidate);
            if (cell == nullptr)
                continue;

            const auto ring = SagaEngine::World::Partition::ClassifyCell(
                focalCoord, candidate, config_.boundary);

            if (ring == SagaEngine::World::Partition::BoundaryRing::Outside)
                continue;

            const std::uint64_t key =
                SagaEngine::World::Partition::PackCellCoord(candidate);

            auto it = trackedCells_.find(key);
            if (it == trackedCells_.end())
            {
                // First touch — cap the tracked-cell map so a
                // pathological boundary configuration cannot grow
                // the table without bound.
                if (trackedCells_.size() >= config_.maxTrackedCells)
                    continue;

                PromoteCellToPrefetching(*cell, ring, currentTick, lastTickStats_);
            }
            else
            {
                // Refresh the last-seen tick so the drain pass
                // knows the cell is still inside the apron.
                it->second.lastSeenTick = currentTick;
                it->second.lastRing     = ring;

                if (it->second.state == SagaEngine::World::Partition::CellState::Draining)
                {
                    // Player came back before the grace period
                    // elapsed — promote straight back to Loaded.
                    it->second.state =
                        SagaEngine::World::Partition::CellState::Loaded;
                    if (config_.logStateTransitions)
                    {
                        LOG_INFO(kLogTag,
                                 "ResourceStream: cell (%d,%d) Draining → Loaded",
                                 candidate.x, candidate.z);
                    }
                }
                else if (it->second.state ==
                         SagaEngine::World::Partition::CellState::Prefetching)
                {
                    // Poll the residency handles — if every handle
                    // is ready and ok, the cell can transition to
                    // Loaded.
                    bool allReady = !it->second.residencyHandles.empty();
                    for (const auto& handle : it->second.residencyHandles)
                    {
                        if (!handle.IsValid() || !handle.IsReady())
                        {
                            allReady = false;
                            break;
                        }
                    }
                    if (allReady)
                    {
                        it->second.state =
                            SagaEngine::World::Partition::CellState::Loaded;
                        ++lastTickStats_.cellsPromotedToLoaded;
                        if (config_.logStateTransitions)
                        {
                            LOG_INFO(kLogTag,
                                     "ResourceStream: cell (%d,%d) Prefetching → Loaded",
                                     candidate.x, candidate.z);
                        }
                    }
                }
            }
        }
    }

    // Second pass — drain tracked cells that were not touched this
    // tick.  We collect the keys first so the main loop does not
    // invalidate iterators while we erase.
    std::vector<std::uint64_t> toEvict;
    for (auto& [key, tracked] : trackedCells_)
    {
        if (tracked.lastSeenTick == currentTick)
            continue;

        if (tracked.state == SagaEngine::World::Partition::CellState::Loaded ||
            tracked.state == SagaEngine::World::Partition::CellState::Prefetching)
        {
            BeginDrainingCell(tracked, currentTick, lastTickStats_);
            continue;
        }

        if (tracked.state == SagaEngine::World::Partition::CellState::Draining)
        {
            // Drain grace period elapsed?  If so, schedule eviction.
            const std::uint64_t age = currentTick - tracked.lastSeenTick;
            if (age >= config_.boundary.drainGraceTicks)
                toEvict.push_back(key);
        }
    }

    for (std::uint64_t key : toEvict)
        EvictCell(key, lastTickStats_);

    return lastTickStats_;
}

// ─── SelectLod ─────────────────────────────────────────────────────────────

std::uint8_t ResourceStream::SelectLod(const LodQuery& query) const noexcept
{
    return lodManager_.Select(query, CurrentPressure());
}

// ─── PromoteCellToPrefetching ──────────────────────────────────────────────

void ResourceStream::PromoteCellToPrefetching(
    const SagaEngine::World::Partition::Cell& cell,
    SagaEngine::World::Partition::BoundaryRing ring,
    std::uint64_t                              currentTick,
    ResourceStreamTickStats&                   stats)
{
    const std::uint64_t key =
        SagaEngine::World::Partition::PackCellCoord(cell.coord);

    TrackedCell tracked;
    tracked.coord        = cell.coord;
    tracked.state        = SagaEngine::World::Partition::CellState::Prefetching;
    tracked.lastRing     = ring;
    tracked.lastSeenTick = currentTick;
    tracked.residencyHandles.reserve(cell.assetRefs.size());

    const auto priority =
        SagaEngine::World::Partition::PriorityForRing(ring, config_.boundary);

    // Issue the prefetches.  Each asset gets its own handle; the
    // tracked cell holds the handles alive so the resource manager
    // keeps the refcount above zero until the player leaves.
    for (const auto& ref : cell.assetRefs)
    {
        AcquireAsset(tracked, ref, priority, stats);
    }

    // If the cell has no assets (an empty placeholder) it is
    // already "loaded" — avoid leaving it permanently stuck in
    // Prefetching.
    if (tracked.residencyHandles.empty())
    {
        tracked.state = SagaEngine::World::Partition::CellState::Loaded;
        ++stats.cellsPromotedToLoaded;
    }
    else
    {
        ++stats.cellsPromotedToPrefetching;
    }

    if (config_.logStateTransitions)
    {
        LOG_INFO(kLogTag,
                 "ResourceStream: cell (%d,%d) Dormant → %s (%zu assets)",
                 cell.coord.x, cell.coord.z,
                 CellStateName(tracked.state),
                 tracked.residencyHandles.size());
    }

    trackedCells_.emplace(key, std::move(tracked));
}

// ─── BeginDrainingCell ─────────────────────────────────────────────────────

void ResourceStream::BeginDrainingCell(TrackedCell&             tracked,
                                       std::uint64_t            currentTick,
                                       ResourceStreamTickStats& stats)
{
    if (tracked.state == SagaEngine::World::Partition::CellState::Draining)
        return;

    tracked.state        = SagaEngine::World::Partition::CellState::Draining;
    tracked.lastSeenTick = currentTick;
    ++stats.cellsDemotedToDraining;

    if (config_.logStateTransitions)
    {
        LOG_INFO(kLogTag,
                 "ResourceStream: cell (%d,%d) → Draining (tick=%llu)",
                 tracked.coord.x, tracked.coord.z,
                 static_cast<unsigned long long>(currentTick));
    }
}

// ─── EvictCell ─────────────────────────────────────────────────────────────

void ResourceStream::EvictCell(std::uint64_t            key,
                               ResourceStreamTickStats& stats)
{
    auto it = trackedCells_.find(key);
    if (it == trackedCells_.end())
        return;

    if (config_.logStateTransitions)
    {
        LOG_INFO(kLogTag,
                 "ResourceStream: cell (%d,%d) evicted (%zu handles released)",
                 it->second.coord.x, it->second.coord.z,
                 it->second.residencyHandles.size());
    }

    // Clearing the tracked cell drops its residency handles which
    // decrements the resource manager refcounts; the manager
    // handles LRU eviction from there.
    trackedCells_.erase(it);
    ++stats.cellsEvicted;
}

// ─── AcquireAsset ──────────────────────────────────────────────────────────

void ResourceStream::AcquireAsset(
    TrackedCell&                                  tracked,
    const SagaEngine::World::Partition::CellAssetRef& ref,
    SagaEngine::Resources::StreamingPriority      priority,
    ResourceStreamTickStats&                      stats)
{
    auto handle = resourceManager_.Acquire(ref.assetId, ref.kind, priority);
    ++stats.prefetchRequestsIssued;

    if (!handle.IsValid())
    {
        ++stats.cellsFailedToPrefetch;
        LOG_WARN(kLogTag,
                 "ResourceStream: failed to acquire assetId=%llu kind=%u",
                 static_cast<unsigned long long>(ref.assetId),
                 static_cast<unsigned>(ref.kind));
        return;
    }

    tracked.residencyHandles.push_back(std::move(handle));
}

// ─── CurrentPressure ───────────────────────────────────────────────────────

float ResourceStream::CurrentPressure() const noexcept
{
    return budget_.Pressure();
}

} // namespace SagaEngine::World::Streaming
