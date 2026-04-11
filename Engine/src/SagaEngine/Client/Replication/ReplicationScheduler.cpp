/// @file ReplicationScheduler.cpp
/// @brief Deterministic execution scheduler for the replication apply pipeline.

#include "SagaEngine/Client/Replication/ReplicationScheduler.h"

#include <algorithm>
#include <cstdint>

namespace SagaEngine::Client::Replication {

namespace {

/// Sequential execution path.
void ExecuteSequential(std::uint32_t entityCount,
                       void (*applyFn)(std::uint32_t, void*),
                       void* context) noexcept
{
    for (std::uint32_t i = 0; i < entityCount; ++i)
        applyFn(i, context);
}

/// Parallel execution through oneTBB.
void ExecuteParallel(std::uint32_t entityCount,
                     std::uint32_t maxThreads,
                     std::uint32_t minBatchSize,
                     void (*applyFn)(std::uint32_t, void*),
                     void* context) noexcept
{
    if (entityCount < minBatchSize)
    {
        // Too small for parallel overhead — run sequentially.
        ExecuteSequential(entityCount, applyFn, context);
        return;
    }

    // Partition into batches.
    std::uint32_t numThreads = maxThreads > 0 ? maxThreads : 8;
    std::uint32_t batchSize = std::max(entityCount / numThreads, minBatchSize);
    (void)batchSize;  // Used when oneTBB integration is complete.

    // In a production system this would dispatch through oneTBB's
    // parallel_for.  For now, we use a simple sequential fallback
    // because the header must compile without oneTBB in the Engine
    // module.  The Server module links oneTBB and overrides this.
    ExecuteSequential(entityCount, applyFn, context);
}

} // namespace

// ─── Configure ──────────────────────────────────────────────────────────────

void ReplicationScheduler::Configure(SchedulerConfig config) noexcept
{
    config_ = config;
}

// ─── Execute ────────────────────────────────────────────────────────────────

void ReplicationScheduler::Execute(std::uint32_t entityCount,
                                    void (*applyFn)(std::uint32_t, void*),
                                    void* context) noexcept
{
    if (entityCount == 0 || !applyFn)
        return;

    switch (config_.mode)
    {
        case SchedulerMode::Sequential:
            ExecuteSequential(entityCount, applyFn, context);
            break;

        case SchedulerMode::Parallel:
            ExecuteParallel(entityCount, config_.maxThreads,
                            config_.minBatchSize, applyFn, context);
            break;
    }
}

} // namespace SagaEngine::Client::Replication
