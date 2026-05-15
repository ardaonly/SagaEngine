/// @file RollbackSnapshotCache.h
/// @brief Baseline snapshot cache for rollback and time-travel debug.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Stores the last N full baseline snapshots so the client can
///          rewind to a previous authoritative state for prediction
///          correction, desync recovery, or debug inspection.  Without
///          this cache, a failed apply forces a full resync from the
///          server — a costly operation that this cache avoids.
///
/// Design rules:
///   - Ring buffer of snapshots, ordered by server tick ascending.
///   - Each snapshot is a raw byte blob (serialised WorldState).
///   - Access is O(1) by tick lookup via a sorted index.
///   - Memory bounded: configurable max entries and total byte cap.

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Stored snapshot ───────────────────────────────────────────────────────

/// One baseline snapshot stored in the rollback cache.
struct StoredBaselineSnapshot
{
    ServerTick                  serverTick = kInvalidTick;
    std::vector<std::uint8_t>   payload;  ///< Serialised WorldState blob.
    std::uint64_t               capturedAtTick = 0;  ///< Client tick when stored.
};

// ─── Rollback cache config ─────────────────────────────────────────────────

struct RollbackCacheConfig
{
    /// Maximum number of baseline snapshots retained.
    std::uint32_t maxEntries = 8;

    /// Total byte budget for the cache.  Oldest entries are evicted
    /// when the budget is exceeded.
    std::uint64_t maxTotalBytes = 64 * 1024 * 1024;  // 64 MiB.
};

// ─── Rollback cache ────────────────────────────────────────────────────────

/// Thread-affine ring buffer of baseline snapshots.
///
/// The pipeline stores each successfully-applied full snapshot here.
/// When a delta apply fails, the cache provides the most recent
/// valid baseline so the client can roll back locally instead of
/// requesting a full resync from the server.
class RollbackSnapshotCache
{
public:
    RollbackSnapshotCache() = default;
    ~RollbackSnapshotCache() = default;

    RollbackSnapshotCache(const RollbackSnapshotCache&)            = delete;
    RollbackSnapshotCache& operator=(const RollbackSnapshotCache&) = delete;

    /// Configure the cache.  Must be called before use.
    void Configure(RollbackCacheConfig config = {}) noexcept;

    /// Store a new baseline snapshot.  Evicts oldest entries if
    /// capacity or byte budget is exceeded.
    void Store(ServerTick tick, std::vector<std::uint8_t>&& payload,
               std::uint64_t clientTick) noexcept;

    /// Retrieve the most recent snapshot at or before the given tick.
    /// Returns nullptr if no matching snapshot exists.
    [[nodiscard]] const StoredBaselineSnapshot* FindAtOrBefore(ServerTick tick) const noexcept;

    /// Retrieve the most recent snapshot in the cache.
    [[nodiscard]] const StoredBaselineSnapshot* Latest() const noexcept;

    /// Remove all entries older than the given tick.
    void PruneOlderThan(ServerTick tick) noexcept;

    [[nodiscard]] std::uint32_t EntryCount() const noexcept { return static_cast<std::uint32_t>(entries_.size()); }
    [[nodiscard]] std::uint64_t TotalBytes() const noexcept { return totalBytes_; }
    void Clear() noexcept;

private:
    RollbackCacheConfig config_{};
    std::vector<StoredBaselineSnapshot> entries_;  ///< Sorted by serverTick ascending.
    std::uint64_t                       totalBytes_ = 0;

    void EvictOldest() noexcept;
};

} // namespace SagaEngine::Client::Replication
