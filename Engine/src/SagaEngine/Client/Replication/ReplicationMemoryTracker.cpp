/// @file ReplicationMemoryTracker.cpp
/// @brief Memory pressure monitoring for the replication pipeline.

#include "SagaEngine/Client/Replication/ReplicationMemoryTracker.h"

namespace SagaEngine::Client::Replication {

// ─── Singleton ──────────────────────────────────────────────────────────────

ReplicationMemoryTracker& ReplicationMemoryTracker::Instance() noexcept
{
    static ReplicationMemoryTracker instance;
    return instance;
}

// ─── Recording ──────────────────────────────────────────────────────────────

void ReplicationMemoryTracker::UpdateStagingBytes(std::uint64_t bytes) noexcept
{
    stagingBytes_.store(bytes, std::memory_order_relaxed);

    // Update peak with compare-exchange to avoid regressions.
    std::uint64_t current = stagingPeakBytes_.load(std::memory_order_relaxed);
    while (bytes > current)
    {
        if (stagingPeakBytes_.compare_exchange_weak(current, bytes, std::memory_order_relaxed))
            break;
    }
}

void ReplicationMemoryTracker::UpdateRollbackCacheBytes(std::uint64_t bytes) noexcept
{
    rollbackCacheBytes_.store(bytes, std::memory_order_relaxed);

    std::uint64_t current = rollbackCachePeak_.load(std::memory_order_relaxed);
    while (bytes > current)
    {
        if (rollbackCachePeak_.compare_exchange_weak(current, bytes, std::memory_order_relaxed))
            break;
    }
}

void ReplicationMemoryTracker::RecordFullSnapshotReallocs(std::uint32_t count) noexcept
{
    lastFullReallocs_.store(count, std::memory_order_relaxed);
    totalReallocs_.fetch_add(count, std::memory_order_relaxed);
}

void ReplicationMemoryTracker::RecordDeltaApplyReallocs(std::uint32_t count) noexcept
{
    lastDeltaReallocs_.store(count, std::memory_order_relaxed);
    totalReallocs_.fetch_add(count, std::memory_order_relaxed);
}

void ReplicationMemoryTracker::RecordBudgetRejection() noexcept
{
    budgetRejections_.fetch_add(1, std::memory_order_relaxed);
}

// ─── Snapshot ───────────────────────────────────────────────────────────────

ReplicationMemorySnapshot ReplicationMemoryTracker::Snapshot() const noexcept
{
    ReplicationMemorySnapshot s;
    s.stagingBytes                = stagingBytes_.load(std::memory_order_relaxed);
    s.stagingPeakBytes            = stagingPeakBytes_.load(std::memory_order_relaxed);
    s.rollbackCacheBytes          = rollbackCacheBytes_.load(std::memory_order_relaxed);
    s.rollbackCachePeakBytes      = rollbackCachePeak_.load(std::memory_order_relaxed);
    s.lastFullSnapshotReallocs    = lastFullReallocs_.load(std::memory_order_relaxed);
    s.lastDeltaApplyReallocs      = lastDeltaReallocs_.load(std::memory_order_relaxed);
    s.totalReallocs               = totalReallocs_.load(std::memory_order_relaxed);
    s.budgetRejections            = budgetRejections_.load(std::memory_order_relaxed);
    return s;
}

void ReplicationMemoryTracker::Reset() noexcept
{
    stagingBytes_.store(0, std::memory_order_relaxed);
    stagingPeakBytes_.store(0, std::memory_order_relaxed);
    rollbackCacheBytes_.store(0, std::memory_order_relaxed);
    rollbackCachePeak_.store(0, std::memory_order_relaxed);
    lastFullReallocs_.store(0, std::memory_order_relaxed);
    lastDeltaReallocs_.store(0, std::memory_order_relaxed);
    totalReallocs_.store(0, std::memory_order_relaxed);
    budgetRejections_.store(0, std::memory_order_relaxed);
}

} // namespace SagaEngine::Client::Replication
