/// @file ReplicationMemoryTracker.h
/// @brief Memory pressure monitoring for the replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Tracks allocation pressure caused by snapshot apply
///          operations, records high-water marks, and provides
///          diagnostics so operators can size budgets correctly.
///          Without visibility into replication memory, budget
///          configuration is guesswork.
///
/// Design rules:
///   - All counters are atomic so the hot path can increment without
///     contention.
///   - High-water marks are updated with compare-exchange so they
///     never regress under concurrent access.
///   - The tracker does not allocate memory itself — it only observes.

#pragma once

#include <atomic>
#include <cstdint>

namespace SagaEngine::Client::Replication {

/// Memory pressure snapshot for the replication pipeline.
struct ReplicationMemorySnapshot
{
    /// Current bytes held in the patch journal staging area.
    std::uint64_t stagingBytes = 0;

    /// Peak staging bytes observed since startup.
    std::uint64_t stagingPeakBytes = 0;

    /// Current bytes in the rollback snapshot cache.
    std::uint64_t rollbackCacheBytes = 0;

    /// Peak rollback cache bytes observed since startup.
    std::uint64_t rollbackCachePeakBytes = 0;

    /// Number of component block reallocations during the last
    /// full snapshot apply.
    std::uint32_t lastFullSnapshotReallocs = 0;

    /// Number of component block reallocations during the last
    /// delta apply.
    std::uint32_t lastDeltaApplyReallocs = 0;

    /// Total component block reallocations since startup.
    std::uint64_t totalReallocs = 0;

    /// Number of times the staging area exceeded the byte budget
    /// and triggered early rejection since startup.
    std::uint64_t budgetRejections = 0;
};

/// Global memory tracker for the replication pipeline.
///
/// Thread-safe: all members are atomic.  The replication hot path
/// increments counters with relaxed ordering so there is zero
/// contention.
class ReplicationMemoryTracker
{
public:
    static ReplicationMemoryTracker& Instance() noexcept;

    ReplicationMemoryTracker(const ReplicationMemoryTracker&) = delete;
    ReplicationMemoryTracker& operator=(const ReplicationMemoryTracker&) = delete;

    /// Record current staging bytes (patch journal staging area).
    void UpdateStagingBytes(std::uint64_t bytes) noexcept;

    /// Record rollback cache bytes.
    void UpdateRollbackCacheBytes(std::uint64_t bytes) noexcept;

    /// Record component block reallocations during a full snapshot apply.
    void RecordFullSnapshotReallocs(std::uint32_t count) noexcept;

    /// Record component block reallocations during a delta apply.
    void RecordDeltaApplyReallocs(std::uint32_t count) noexcept;

    /// Record a budget rejection (staging area exceeded byte budget).
    void RecordBudgetRejection() noexcept;

    /// Take a consistent snapshot of all current counters.
    [[nodiscard]] ReplicationMemorySnapshot Snapshot() const noexcept;

    /// Reset all counters to zero.
    void Reset() noexcept;

private:
    ReplicationMemoryTracker() = default;

    std::atomic<std::uint64_t> stagingBytes_       = {0};
    std::atomic<std::uint64_t> stagingPeakBytes_    = {0};
    std::atomic<std::uint64_t> rollbackCacheBytes_  = {0};
    std::atomic<std::uint64_t> rollbackCachePeak_   = {0};
    std::atomic<std::uint32_t> lastFullReallocs_    = {0};
    std::atomic<std::uint32_t> lastDeltaReallocs_   = {0};
    std::atomic<std::uint64_t> totalReallocs_       = {0};
    std::atomic<std::uint64_t> budgetRejections_    = {0};
};

} // namespace SagaEngine::Client::Replication
