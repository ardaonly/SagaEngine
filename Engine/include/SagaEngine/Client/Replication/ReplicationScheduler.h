/// @file ReplicationScheduler.h
/// @brief Deterministic execution scheduler for the replication apply pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Enforces a strict, documented ordering contract for all
///          component applies so that two clients processing the same
///          delta stream always produce identical WorldState bytes,
///          regardless of thread count or timing.  Without this
///          contract, parallel apply introduces non-determinism that
///          breaks prediction reconciliation.
///
/// Design rules:
///   - Single-threaded by default: all applies run sequentially on the
///     game thread.
///   - Optional parallel mode: independent entity groups are applied
///     in parallel through oneTBB, but intra-group ordering is fixed.
///   - Entity iteration order is always ascending by EntityId.
///   - Component apply order within an entity is ascending by
///     ComponentTypeId.
///   - No data races: each entity's component writes are isolated.

#pragma once

#include <cstdint>

namespace SagaEngine::Client::Replication {

/// Scheduling mode for the replication apply pipeline.
enum class SchedulerMode : std::uint8_t
{
    Sequential,     ///< All applies run on the calling thread (default).
    Parallel,       ///< Independent entity groups applied in parallel.
};

/// Scheduler configuration.
struct SchedulerConfig
{
    SchedulerMode mode = SchedulerMode::Sequential;

    /// Maximum number of worker threads for parallel apply.
    /// Zero means "use all available cores".
    std::uint32_t maxThreads = 0;

    /// Minimum number of entities per parallel batch.
    /// Batches smaller than this run sequentially to avoid overhead.
    std::uint32_t minBatchSize = 64;
};

/// Deterministic replication apply scheduler.
///
/// Thread-affine in Sequential mode.  In Parallel mode, the
/// scheduler partitions entities into independent batches and
/// dispatches them through the job system.
class ReplicationScheduler
{
public:
    ReplicationScheduler() = default;
    ~ReplicationScheduler() = default;

    ReplicationScheduler(const ReplicationScheduler&)            = delete;
    ReplicationScheduler& operator=(const ReplicationScheduler&) = delete;

    /// Configure the scheduler.  Must be called before use.
    void Configure(SchedulerConfig config) noexcept;

    /// Execute a batch of entity applies.
    ///
    /// The caller provides a function that applies one entity's
    /// patches.  The scheduler decides whether to run them
    /// sequentially or partition them for parallel execution.
    ///
    /// @param entityCount  Number of entities to apply.
    /// @param applyFn      Function that applies entity at index i.
    ///                     Must be thread-safe in Parallel mode.
    void Execute(std::uint32_t entityCount,
                 void (*applyFn)(std::uint32_t entityIndex, void* context),
                 void* context) noexcept;

    /// Return the configured mode.
    [[nodiscard]] SchedulerMode GetMode() const noexcept { return config_.mode; }

private:
    SchedulerConfig config_{};
};

} // namespace SagaEngine::Client::Replication
