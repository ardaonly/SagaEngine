/// @file MemoryPressureTests.cpp
/// @brief Stress tests for memory pressure, arena allocator limits, and leak detection.
///
/// These tests validate the engine's memory management under extreme pressure:
/// arena budget enforcement, memory tracker leak detection, pool allocator
/// throughput, and GC-free allocation patterns.

#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include "SagaEngine/Core/Memory/PoolAllocator.h"
#include "SagaEngine/Core/Memory/MemoryTracker.h"
#include "SagaEngine/Core/Profiling/Profiler.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <cstring>

using namespace SagaEngine::Core;

// ─── Test fixture ──────────────────────────────────────────────────────────────

class MemoryPressureStressTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Profiler::Instance().Clear();
    }
};

// ─── Test: arena allocator budget enforcement ──────────────────────────────────

TEST_F(MemoryPressureStressTest, ArenaBudgetEnforcement)
{
    SAGA_PROFILE_FUNCTION();

    // Create a small arena (64 KiB)
    ArenaAllocator arena(64u * 1024u);

    std::vector<void*> allocations;
    size_t totalAllocated = 0;

    // Keep allocating until the budget is exhausted
    while (true)
    {
        void* ptr = arena.Allocate(1024, 16); // 1 KiB aligned
        if (ptr == nullptr)
            break;
        allocations.push_back(ptr);
        totalAllocated += 1024;
    }

    // Should have allocated a significant amount but not exceeded budget
    EXPECT_GT(totalAllocated, 0u);
    EXPECT_LE(totalAllocated, arena.Stats().bytesCommitted + 1024);

    // Stats should reflect allocations
    const auto& stats = arena.Stats();
    EXPECT_GT(stats.allocations, 0u);
    EXPECT_EQ(stats.allocationFailures, 1u); // The final failed allocation
}

// ─── Test: arena rewind preserves block commitment ────────────────────────────

TEST_F(MemoryPressureStressTest, ArenaRewindPreservesCommitment)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(128u * 1024u);

    // Allocate and rewind in cycles
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        {
            ScopedArena scope(arena);

            // Allocate varying sizes
            for (int i = 0; i < 50; ++i)
            {
                auto* ptr = arena.Allocate(static_cast<size_t>(i) * 64 + 128, 16);
                ASSERT_NE(ptr, nullptr);
                std::memset(ptr, static_cast<int>(i & 0xFF), 128);
            }
        }
        // Rewind happens here
    }

    const auto& stats = arena.Stats();
    EXPECT_EQ(stats.bytesInUse, 0u);
    // Blocks should be retained (committed memory stays high after first fill)
    EXPECT_GT(stats.bytesCommitted, 0u);
    EXPECT_GE(stats.blockCount, 1u);
}

// ─── Test: memory tracker basic operations ────────────────────────────────────

TEST_F(MemoryPressureStressTest, MemoryTrackerBasicOperations)
{
    SAGA_PROFILE_FUNCTION();

    auto& tracker = MemoryTracker::Instance();

    const size_t initialAlloc = tracker.GetTotalAllocated();
    const size_t initialCount = tracker.GetAllocationCount();

    // Allocate and track
    void* ptr1 = malloc(256);
    tracker.TrackAllocation(ptr1, 256, __FILE__, __LINE__, "TestPtr1");

    EXPECT_EQ(tracker.GetTotalAllocated(), initialAlloc + 256);
    EXPECT_EQ(tracker.GetAllocationCount(), initialCount + 1);

    void* ptr2 = malloc(512);
    tracker.TrackAllocation(ptr2, 512, __FILE__, __LINE__, "TestPtr2");

    EXPECT_EQ(tracker.GetTotalAllocated(), initialAlloc + 256 + 512);
    EXPECT_EQ(tracker.GetAllocationCount(), initialCount + 2);

    // Free
    tracker.TrackDeallocation(ptr1);
    free(ptr1);

    EXPECT_EQ(tracker.GetTotalAllocated(), initialAlloc + 512);
    EXPECT_EQ(tracker.GetAllocationCount(), initialCount + 1);

    tracker.TrackDeallocation(ptr2);
    free(ptr2);

    EXPECT_EQ(tracker.GetTotalAllocated(), initialAlloc);
    EXPECT_EQ(tracker.GetAllocationCount(), initialCount);
}

// ─── Test: pool allocator throughput ──────────────────────────────────────────

TEST_F(MemoryPressureStressTest, PoolAllocatorThroughput)
{
    SAGA_PROFILE_FUNCTION();

    constexpr size_t kPoolCapacity = 10'000;

    // PoolAllocator is typed with fixed block count
    struct PoolObject { uint8_t data[64]; };
    PoolAllocator<PoolObject, kPoolCapacity> pool;

    const auto t0 = std::chrono::steady_clock::now();

    std::vector<PoolObject*> ptrs;
    ptrs.reserve(kPoolCapacity);

    // Allocate all objects from pool
    for (size_t i = 0; i < kPoolCapacity; ++i)
    {
        PoolObject* ptr = pool.Allocate();
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Pool should be exhausted
    PoolObject* overflow = pool.Allocate();
    EXPECT_EQ(overflow, nullptr);

    // Free all
    for (PoolObject* ptr : ptrs)
    {
        pool.Deallocate(ptr);
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double allocsPerSec = static_cast<double>(kPoolCapacity) / (elapsedMs / 1000.0);

    // Should handle at least 100k allocs/sec
    EXPECT_GT(allocsPerSec, 100'000.0);
}

// ─── Test: concurrent arena usage (one per thread) ────────────────────────────

TEST_F(MemoryPressureStressTest, ConcurrentArenaUsage)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kThreads = 8;
    constexpr int kAllocsPerThread = 10'000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    std::atomic<size_t> totalAllocated{0};

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([t, kAllocsPerThread, &totalAllocated]()
        {
            ArenaAllocator arena(256u * 1024u); // 256 KiB per thread

            for (int i = 0; i < kAllocsPerThread; ++i)
            {
                auto* ptr = arena.Allocate(32, 16);
                if (ptr != nullptr)
                {
                    std::memset(ptr, static_cast<int>(t), 32);
                    totalAllocated.fetch_add(32, std::memory_order_relaxed);
                }

                // Rewind every 1000 allocations to simulate per-tick reset
                if (i % 1000 == 999)
                {
                    arena.Reset();
                }
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_GT(totalAllocated.load(), 0u);
}

// ─── Test: large allocation stress ────────────────────────────────────────────

TEST_F(MemoryPressureStressTest, LargeAllocationStress)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(1u * 1024u * 1024u); // 1 MiB blocks

    // Allocate objects that exceed block size — each gets its own block
    constexpr size_t kLargeAlloc = 2u * 1024u * 1024u; // 2 MiB
    constexpr int kCount = 5;

    std::vector<void*> ptrs;
    ptrs.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        void* ptr = arena.Allocate(kLargeAlloc, 16);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    const auto& stats = arena.Stats();
    // Should have at least kCount + 1 blocks (initial + one per oversized alloc)
    EXPECT_GE(stats.blockCount, static_cast<uint32_t>(kCount));
    EXPECT_GE(stats.bytesCommitted, static_cast<uint64_t>(kCount) * kLargeAlloc);
}

// ─── Test: memory fragmentation simulation ────────────────────────────────────

TEST_F(MemoryPressureStressTest, FragmentationSimulation)
{
    SAGA_PROFILE_FUNCTION();

    // Pool allocator — simulates fixed-size object allocation without fragmentation
    constexpr size_t kPoolCapacity = 5'000;

    struct FragObject { uint8_t data[128]; };
    PoolAllocator<FragObject, kPoolCapacity> pool;

    std::vector<FragObject*> allPtrs;
    allPtrs.reserve(kPoolCapacity);

    // Fill the pool
    for (size_t i = 0; i < kPoolCapacity; ++i)
    {
        FragObject* ptr = pool.Allocate();
        ASSERT_NE(ptr, nullptr);
        allPtrs.push_back(ptr);
    }

    // Free every other allocation
    for (size_t i = 0; i < kPoolCapacity; i += 2)
    {
        pool.Deallocate(allPtrs[i]);
        allPtrs[i] = nullptr;
    }

    // Should be able to allocate half the pool again
    size_t reallocCount = 0;
    for (size_t i = 0; i < kPoolCapacity; i += 2)
    {
        FragObject* ptr = pool.Allocate();
        if (ptr != nullptr)
        {
            allPtrs[i] = ptr;
            reallocCount++;
        }
    }

    // Pool allocator recycles freed slots, so reallocation should succeed
    EXPECT_EQ(reallocCount, kPoolCapacity / 2);
}

// ─── Test: ArenaAllocator marker/rewind correctness ───────────────────────────

TEST_F(MemoryPressureStressTest, MarkerRewindCorrectness)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(64u * 1024u);

    // Allocate some baseline data
    auto* baseline = arena.Allocate(1024, 16);
    ASSERT_NE(baseline, nullptr);
    std::memset(baseline, 0xAA, 1024);

    // Mark
    auto marker = arena.Mark();

    // Allocate more data after marker
    std::vector<void*> postMarker;
    for (int i = 0; i < 10; ++i)
    {
        void* ptr = arena.Allocate(512, 16);
        ASSERT_NE(ptr, nullptr);
        postMarker.push_back(ptr);
        std::memset(ptr, static_cast<int>(i), 512);
    }

    const size_t bytesBeforeRewind = arena.Stats().bytesInUse;

    // Rewind to marker
    arena.Rewind(marker);

    const size_t bytesAfterRewind = arena.Stats().bytesInUse;

    // Bytes in use should be back to baseline level
    EXPECT_LT(bytesAfterRewind, bytesBeforeRewind);
    EXPECT_EQ(bytesAfterRewind, 1024u); // Only the baseline should remain

    // Baseline data should still be valid (pointer still usable)
    const uint8_t* baselineBytes = static_cast<const uint8_t*>(baseline);
    for (int i = 0; i < 1024; ++i)
    {
        EXPECT_EQ(baselineBytes[i], 0xAA);
    }

    // Allocate again — should reuse the space freed by rewind
    void* newPtr = arena.Allocate(4096, 16);
    ASSERT_NE(newPtr, nullptr);
}

// ─── Test: ScopedArena RAII correctness ───────────────────────────────────────

TEST_F(MemoryPressureStressTest, ScopedArenaRAII)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(128u * 1024u);

    // Pre-allocate baseline
    auto* baseline = arena.Allocate(256, 16);
    ASSERT_NE(baseline, nullptr);

    const size_t bytesBeforeScope = arena.Stats().bytesInUse;

    {
        ScopedArena scope(arena);

        // Allocate within scope
        for (int i = 0; i < 20; ++i)
        {
            void* ptr = arena.Allocate(1024, 16);
            ASSERT_NE(ptr, nullptr);
        }

        const size_t bytesInScope = arena.Stats().bytesInUse;
        EXPECT_GT(bytesInScope, bytesBeforeScope);
    }
    // Scope destroyed — arena rewound

    const size_t bytesAfterScope = arena.Stats().bytesInUse;
    EXPECT_EQ(bytesAfterScope, bytesBeforeScope);
}

// ─── Test: memory tracker with high-frequency alloc/dealloc ───────────────────

TEST_F(MemoryPressureStressTest, HighFrequencyAllocDealloc)
{
    SAGA_PROFILE_FUNCTION();

    auto& tracker = MemoryTracker::Instance();

    const size_t initialAlloc = tracker.GetTotalAllocated();
    constexpr int kIterations = 100'000;

    for (int i = 0; i < kIterations; ++i)
    {
        void* ptr = malloc(64);
        tracker.TrackAllocation(ptr, 64, __FILE__, __LINE__, "HFTest");
        tracker.TrackDeallocation(ptr);
        free(ptr);
    }

    // After all alloc/dealloc cycles, should be back to baseline
    EXPECT_EQ(tracker.GetTotalAllocated(), initialAlloc);
}
