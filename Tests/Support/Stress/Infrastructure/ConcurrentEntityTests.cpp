/// @file ConcurrentEntityTests.cpp
/// @brief Stress tests for concurrent entity creation, deletion, and mixed operations.
///
/// These tests validate WorldState correctness under high-throughput entity
/// lifecycle pressure. WorldState itself is NOT thread-safe; the tests
/// exercise the arena allocator and concurrent patterns that a multi-threaded
/// server would use (one WorldState per thread, merge at end of tick).

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Threading/JobSystem.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <unordered_set>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;

// ─── Stress test component types ──────────────────────────────────────────────

struct StressPosition
{
    float x = 0.f, y = 0.f, z = 0.f;
    uint32_t padding = 0;
};

struct StressVelocity
{
    float vx = 0.f, vy = 0.f, vz = 0.f;
    uint32_t padding = 0;
};

struct StressHealth
{
    float current = 100.f;
    float max = 100.f;
};

// ─── Helper: register components idempotently ─────────────────────────────────

static void RegisterStressComponents()
{
    using CR = ECS::ComponentRegistry;

    if (!CR::Instance().IsRegistered<StressPosition>())
        CR::Instance().Register<StressPosition>(2001u, "StressPosition");

    if (!CR::Instance().IsRegistered<StressVelocity>())
        CR::Instance().Register<StressVelocity>(2002u, "StressVelocity");

    if (!CR::Instance().IsRegistered<StressHealth>())
        CR::Instance().Register<StressHealth>(2003u, "StressHealth");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class ConcurrentEntityStressTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterStressComponents();
        Profiler::Instance().Clear();
    }
};

// ─── Single-threaded baseline: massive create/destroy cycle ───────────────────

TEST_F(ConcurrentEntityStressTest, SingleThreadCreateDestroy100k)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    const int kCount = 100'000;

    std::vector<ECS::EntityHandle> entities;
    entities.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        entities.push_back(world.CreateEntity());
        world.AddComponent<StressPosition>(entities.back().id, {0.f, 0.f, 0.f});
    }

    EXPECT_EQ(world.EntityCount(), kCount);

    for (const auto& e : entities)
        world.DestroyEntity(e.id);

    EXPECT_EQ(world.EntityCount(), 0u);
}

// ─── Multi-threaded: per-world entity creation then merge ─────────────────────

TEST_F(ConcurrentEntityStressTest, MultiThreadPerWorldCreation)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kThreads = 8;
    constexpr int kEntitiesPerThread = 10'000;

    std::vector<WorldState> worlds(kThreads);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&worlds, t, kEntitiesPerThread]()
        {
            WorldState& world = worlds[static_cast<size_t>(t)];
            for (int i = 0; i < kEntitiesPerThread; ++i)
            {
                const auto entity = world.CreateEntity();
                world.AddComponent<StressPosition>(entity.id,
                    {static_cast<float>(t), static_cast<float>(i), 0.f});
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    // Verify each world has the expected count
    for (int t = 0; t < kThreads; ++t)
    {
        EXPECT_EQ(worlds[static_cast<size_t>(t)].EntityCount(),
                  static_cast<uint32_t>(kEntitiesPerThread));
    }
}

// ─── Multi-threaded: create + destroy mixed operations per world ──────────────

TEST_F(ConcurrentEntityStressTest, MultiThreadMixedCreateDestroy)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kThreads = 4;
    constexpr int kOpsPerThread = 50'000;

    std::vector<WorldState> worlds(kThreads);
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    std::vector<std::atomic<int>> createdCounts(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&worlds, t, kOpsPerThread, &createdCounts]()
        {
            WorldState& world = worlds[static_cast<size_t>(t)];
            std::vector<ECS::EntityId> liveEntities;
            liveEntities.reserve(kOpsPerThread / 2);
            std::mt19937 rng(static_cast<uint32_t>(t * 1000 + 42));
            std::uniform_int_distribution<int> opDist(0, 99);

            int created = 0;

            for (int i = 0; i < kOpsPerThread; ++i)
            {
                const int op = opDist(rng);
                if (op < 60 || liveEntities.empty())
                {
                    // Create
                    const auto entity = world.CreateEntity();
                    world.AddComponent<StressPosition>(entity.id,
                        {static_cast<float>(i), 0.f, 0.f});
                    if (op < 80)
                    {
                        world.AddComponent<StressVelocity>(entity.id,
                            {1.f, 0.f, 0.f});
                    }
                    liveEntities.push_back(entity.id);
                    created++;
                }
                else
                {
                    // Destroy random
                    std::uniform_int_distribution<size_t> idxDist(0, liveEntities.size() - 1);
                    const size_t idx = idxDist(rng);
                    world.DestroyEntity(liveEntities[idx]);
                    liveEntities[idx] = liveEntities.back();
                    liveEntities.pop_back();
                }
            }

            createdCounts[static_cast<size_t>(t)] = created;
        });
    }

    for (auto& thread : threads)
        thread.join();

    // Verify all worlds are consistent
    for (int t = 0; t < kThreads; ++t)
    {
        const auto& world = worlds[static_cast<size_t>(t)];
        const int created = createdCounts[static_cast<size_t>(t)].load();
        // Entity count should not exceed created count
        EXPECT_LE(world.EntityCount(), static_cast<uint32_t>(created));
    }
}

// ─── JobSystem-based concurrent entity creation ───────────────────────────────

TEST_F(ConcurrentEntityStressTest, JobSystemEntityCreation)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kJobs = 16;
    constexpr int kEntitiesPerJob = 5'000;

    JobSystem jobsys(static_cast<size_t>(std::thread::hardware_concurrency()));
    std::vector<WorldState> worlds(kJobs);
    std::vector<JobHandle> handles;
    handles.reserve(kJobs);

    for (int j = 0; j < kJobs; ++j)
    {
        handles.push_back(jobsys.Schedule([&worlds, j, kEntitiesPerJob]()
        {
            WorldState& world = worlds[static_cast<size_t>(j)];
            for (int i = 0; i < kEntitiesPerJob; ++i)
            {
                const auto entity = world.CreateEntity();
                world.AddComponent<StressPosition>(entity.id,
                    {static_cast<float>(j), static_cast<float>(i), 0.f});
            }
        }));
    }

    jobsys.WaitForAll();

    uint32_t totalEntities = 0;
    for (int j = 0; j < kJobs; ++j)
    {
        totalEntities += worlds[static_cast<size_t>(j)].EntityCount();
    }

    EXPECT_EQ(totalEntities, static_cast<uint32_t>(kJobs * kEntitiesPerJob));
}

// ─── Arena allocator stress: rapid allocate/rewind cycles ─────────────────────

TEST_F(ConcurrentEntityStressTest, ArenaAllocateRewindStress)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(256u * 1024u); // 256 KiB blocks
    constexpr int kIterations = 100'000;

    for (int i = 0; i < kIterations; ++i)
    {
        {
            ScopedArena scope(arena);
            auto* pos = arena.Create<StressPosition>(1.f, 2.f, 3.f);
            ASSERT_NE(pos, nullptr);
            EXPECT_FLOAT_EQ(pos->x, 1.f);
            EXPECT_FLOAT_EQ(pos->y, 2.f);
            EXPECT_FLOAT_EQ(pos->z, 3.f);

            // Allocate additional objects within the same scope
            for (int j = 0; j < 10; ++j)
            {
                auto* vel = arena.Create<StressVelocity>(
                    static_cast<float>(j), 0.f, 0.f);
                ASSERT_NE(vel, nullptr);
            }
        }
        // Arena rewinds here — all bytes should be back to zero
    }

    // After all iterations, the arena should have minimal bytes in use
    const auto& stats = arena.Stats();
    EXPECT_EQ(stats.bytesInUse, 0u);
    EXPECT_GT(stats.bytesAllocated, 0u);
}

// ─── Entity handle versioning stress ──────────────────────────────────────────

TEST_F(ConcurrentEntityStressTest, HandleVersioningUnderRecycle)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    std::vector<ECS::EntityHandle> handles;

    // Create and destroy entities in cycles, then verify stale handles are invalid
    for (int cycle = 0; cycle < 100; ++cycle)
    {
        handles.push_back(world.CreateEntity());
        world.AddComponent<StressPosition>(handles.back().id,
            {static_cast<float>(cycle), 0.f, 0.f});
    }

    // Destroy all
    for (const auto& h : handles)
        world.DestroyEntity(h.id);

    // Verify all old handles are invalid
    for (const auto& h : handles)
        EXPECT_FALSE(world.IsValid(h));

    // Re-create entities — IDs may be recycled but versions should differ
    std::vector<ECS::EntityHandle> newHandles;
    newHandles.reserve(handles.size());

    for (size_t i = 0; i < handles.size(); ++i)
    {
        newHandles.push_back(world.CreateEntity());
    }

    // New handles should be valid
    for (const auto& h : newHandles)
        EXPECT_TRUE(world.IsValid(h));
}

// ─── Component add/remove stress ──────────────────────────────────────────────

TEST_F(ConcurrentEntityStressTest, ComponentAddRemoveStress)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntities = 10'000;

    std::vector<ECS::EntityId> entities;
    entities.reserve(kEntities);

    for (int i = 0; i < kEntities; ++i)
    {
        entities.push_back(world.CreateEntity().id);
    }

    // Add components to all entities
    for (const auto& id : entities)
    {
        world.AddComponent<StressPosition>(id, {0.f, 0.f, 0.f});
        world.AddComponent<StressVelocity>(id, {0.f, 0.f, 0.f});
        world.AddComponent<StressHealth>(id, {100.f, 100.f});
    }

    EXPECT_EQ(world.TotalComponents(), static_cast<uint32_t>(kEntities * 3));

    // Remove velocity from half the entities
    for (int i = 0; i < kEntities; i += 2)
    {
        world.RemoveComponent<StressVelocity>(entities[static_cast<size_t>(i)]);
    }

    // Verify
    for (int i = 0; i < kEntities; ++i)
    {
        EXPECT_TRUE(world.HasComponent<StressPosition>(entities[static_cast<size_t>(i)]));
        EXPECT_TRUE(world.HasComponent<StressHealth>(entities[static_cast<size_t>(i)]));

        if (i % 2 == 0)
            EXPECT_FALSE(world.HasComponent<StressVelocity>(entities[static_cast<size_t>(i)]));
        else
            EXPECT_TRUE(world.HasComponent<StressVelocity>(entities[static_cast<size_t>(i)]));
    }
}

// ─── Query performance under load ─────────────────────────────────────────────

TEST_F(ConcurrentEntityStressTest, QueryUnderLoad)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntities = 50'000;

    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<StressPosition>(entity.id,
            {static_cast<float>(i), 0.f, 0.f});

        if (i % 3 == 0)
        {
            world.AddComponent<StressVelocity>(entity.id, {1.f, 0.f, 0.f});
        }
        if (i % 5 == 0)
        {
            world.AddComponent<StressHealth>(entity.id, {50.f, 100.f});
        }
    }

    const auto t0 = std::chrono::steady_clock::now();

    // Query all entities with Position + Velocity
    std::vector<ECS::ComponentTypeId> required = {
        ECS::ComponentRegistry::Instance().GetId<StressPosition>(),
        ECS::ComponentRegistry::Instance().GetId<StressVelocity>()
    };
    const auto results = world.Query(required);

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Every third entity has velocity
    EXPECT_EQ(results.size(), static_cast<size_t>(kEntities / 3));
    // Query should be fast — under 50ms for 50k entities
    EXPECT_LT(elapsedMs, 50.0);
}
