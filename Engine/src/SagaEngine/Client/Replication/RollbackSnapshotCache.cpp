/// @file RollbackSnapshotCache.cpp
/// @brief Baseline snapshot cache for rollback and time-travel debug.

#include "SagaEngine/Client/Replication/RollbackSnapshotCache.h"

#include <algorithm>

namespace SagaEngine::Client::Replication {

namespace {

constexpr const char* kLogTag = "Replication";

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void RollbackSnapshotCache::Configure(RollbackCacheConfig config) noexcept
{
    config_ = config;
}

// ─── Store ──────────────────────────────────────────────────────────────────

void RollbackSnapshotCache::Store(ServerTick tick, std::vector<std::uint8_t>&& payload,
                                   std::uint64_t clientTick) noexcept
{
    std::uint64_t snapshotBytes = payload.size();

    // Reject if a single snapshot exceeds the total budget.
    if (snapshotBytes > config_.maxTotalBytes)
        return;

    StoredBaselineSnapshot entry;
    entry.serverTick     = tick;
    entry.payload        = std::move(payload);
    entry.capturedAtTick = clientTick;

    totalBytes_ += entry.payload.size();

    // Insert sorted by serverTick.
    auto it = std::lower_bound(entries_.begin(), entries_.end(), entry,
                               [](const StoredBaselineSnapshot& a,
                                  const StoredBaselineSnapshot& b) {
                                   return a.serverTick < b.serverTick;
                               });
    entries_.insert(it, std::move(entry));

    // Evict until we fit both constraints.
    while (entries_.size() > config_.maxEntries || totalBytes_ > config_.maxTotalBytes)
        EvictOldest();
}

// ─── Find ───────────────────────────────────────────────────────────────────

const StoredBaselineSnapshot* RollbackSnapshotCache::FindAtOrBefore(ServerTick tick) const noexcept
{
    const StoredBaselineSnapshot* best = nullptr;

    for (const auto& entry : entries_)
    {
        if (entry.serverTick <= tick)
            best = &entry;
        else
            break;  // entries are sorted ascending.
    }

    return best;
}

const StoredBaselineSnapshot* RollbackSnapshotCache::Latest() const noexcept
{
    return entries_.empty() ? nullptr : &entries_.back();
}

// ─── Prune ──────────────────────────────────────────────────────────────────

void RollbackSnapshotCache::PruneOlderThan(ServerTick tick) noexcept
{
    auto it = std::remove_if(entries_.begin(), entries_.end(),
                             [tick](const StoredBaselineSnapshot& e) {
                                 return e.serverTick < tick;
                             });

    for (auto pruneIt = it; pruneIt != entries_.end(); ++pruneIt)
        totalBytes_ -= pruneIt->payload.size();

    entries_.erase(it, entries_.end());
}

void RollbackSnapshotCache::Clear() noexcept
{
    entries_.clear();
    totalBytes_ = 0;
}

// ─── Eviction ───────────────────────────────────────────────────────────────

void RollbackSnapshotCache::EvictOldest() noexcept
{
    if (entries_.empty())
        return;

    totalBytes_ -= entries_.front().payload.size();
    entries_.erase(entries_.begin());
}

} // namespace SagaEngine::Client::Replication
