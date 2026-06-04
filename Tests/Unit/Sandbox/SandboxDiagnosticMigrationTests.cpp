/// @file SandboxDiagnosticMigrationTests.cpp
/// @brief Regression coverage migrated from durable sandbox diagnostic behavior.

#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include "SagaEngine/Core/Memory/PoolAllocator.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/ECS/Query.h"
#include "SagaEngine/Simulation/WorldState.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace
{

struct SandboxProbeTransform
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};
static_assert(std::is_trivially_copyable_v<SandboxProbeTransform>);

struct SandboxProbeVelocity
{
    float vx = 0.f;
    float vy = 0.f;
    float vz = 0.f;
};
static_assert(std::is_trivially_copyable_v<SandboxProbeVelocity>);

struct SandboxProbeHealth
{
    float current = 100.f;
    float max = 100.f;
};
static_assert(std::is_trivially_copyable_v<SandboxProbeHealth>);

struct SandboxProbeObject
{
    float data[16] = {};
};

constexpr SagaEngine::ECS::ComponentTypeId kSandboxProbeTransformId = 9101u;
constexpr SagaEngine::ECS::ComponentTypeId kSandboxProbeVelocityId = 9102u;
constexpr SagaEngine::ECS::ComponentTypeId kSandboxProbeHealthId = 9103u;

void RegisterSandboxProbeComponents()
{
    auto& registry = SagaEngine::ECS::ComponentRegistry::Instance();
    if (!registry.IsRegistered<SandboxProbeTransform>())
        registry.Register<SandboxProbeTransform>(kSandboxProbeTransformId, "SandboxProbeTransform");
    if (!registry.IsRegistered<SandboxProbeVelocity>())
        registry.Register<SandboxProbeVelocity>(kSandboxProbeVelocityId, "SandboxProbeVelocity");
    if (!registry.IsRegistered<SandboxProbeHealth>())
        registry.Register<SandboxProbeHealth>(kSandboxProbeHealthId, "SandboxProbeHealth");
}

bool ContainsEntity(const std::vector<SagaEngine::ECS::EntityId>& entities,
                    SagaEngine::ECS::EntityId id)
{
    return std::find(entities.begin(), entities.end(), id) != entities.end();
}

} // namespace

TEST(SandboxDiagnosticMigrationTests, ECSProbeLifecycleAndQueriesHaveRegressionCoverage)
{
    RegisterSandboxProbeComponents();

    SagaEngine::Simulation::WorldState world;
    std::vector<SagaEngine::ECS::EntityId> entities;
    entities.reserve(50);

    for (std::uint32_t i = 0; i < 50; ++i)
    {
        const auto handle = world.CreateEntity();
        world.AddComponent<SandboxProbeTransform>(
            handle.id,
            {static_cast<float>(i), static_cast<float>(i + 1u), static_cast<float>(i + 2u)});
        world.AddComponent<SandboxProbeVelocity>(
            handle.id,
            {static_cast<float>(i) * 0.1f, static_cast<float>(i) * 0.2f, 0.f});

        if ((i % 2u) == 0u)
            world.AddComponent<SandboxProbeHealth>(handle.id, {75.f, 100.f});

        entities.push_back(handle.id);
    }

    EXPECT_EQ(world.EntityCount(), 50u);
    EXPECT_EQ(world.ComponentTypes(), 3u);
    EXPECT_EQ(world.TotalComponents(), 125u);

    SagaEngine::ECS::Query transformVelocityQuery(&world);
    transformVelocityQuery.WithComponent(kSandboxProbeTransformId);
    transformVelocityQuery.WithComponent(kSandboxProbeVelocityId);

    const auto movingEntities = transformVelocityQuery.Execute();
    ASSERT_EQ(movingEntities.size(), 50u);
    EXPECT_TRUE(ContainsEntity(movingEntities, entities.front()));
    EXPECT_TRUE(ContainsEntity(movingEntities, entities.back()));

    const auto healthyEntities = world.Query({kSandboxProbeHealthId});
    EXPECT_EQ(healthyEntities.size(), 25u);

    for (std::size_t i = 0; i < 10; ++i)
        world.DestroyEntity(entities[i]);

    const auto remainingMovingEntities = transformVelocityQuery.Execute();
    EXPECT_EQ(world.EntityCount(), 40u);
    EXPECT_EQ(remainingMovingEntities.size(), 40u);
    EXPECT_FALSE(ContainsEntity(remainingMovingEntities, entities.front()));
    EXPECT_TRUE(ContainsEntity(remainingMovingEntities, entities.back()));
    EXPECT_EQ(world.Query({kSandboxProbeHealthId}).size(), 20u);
}

TEST(SandboxDiagnosticMigrationTests, MemoryDiagAllocatorBehaviorHasRegressionCoverage)
{
    SagaEngine::Core::ArenaAllocator arena(1024);

    const auto initialStats = arena.Stats();
    ASSERT_GE(initialStats.blockCount, 1u);
    ASSERT_GE(initialStats.bytesCommitted, 1024u);

    void* first = arena.Allocate(128, alignof(SandboxProbeObject));
    ASSERT_NE(first, nullptr);

    const auto marker = arena.Mark();
    void* second = arena.Allocate(256, alignof(SandboxProbeObject));
    ASSERT_NE(second, nullptr);
    EXPECT_GT(arena.Stats().bytesInUse, 128u);
    EXPECT_EQ(arena.Stats().allocations, 2u);

    arena.Rewind(marker);
    EXPECT_LE(arena.Stats().bytesInUse, arena.Stats().bytesAllocated);

    arena.Reset();
    EXPECT_EQ(arena.Stats().bytesInUse, 0u);
    EXPECT_EQ(arena.Stats().allocations, 0u);

    SagaEngine::Core::PoolAllocator<SandboxProbeObject, 4> pool;
    auto* a = pool.Allocate();
    auto* b = pool.Allocate();
    auto* c = pool.Allocate();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(pool.GetAllocatedCount(), 3u);
    EXPECT_EQ(pool.GetFreeCount(), 1u);

    pool.Deallocate(b);
    EXPECT_EQ(pool.GetAllocatedCount(), 2u);
    EXPECT_EQ(pool.GetFreeCount(), 2u);

    auto* reused = pool.Allocate();
    EXPECT_EQ(reused, b);
    EXPECT_EQ(pool.GetAllocatedCount(), 3u);
    EXPECT_EQ(pool.GetFreeCount(), 1u);

    pool.Deallocate(a);
    pool.Deallocate(c);
    pool.Deallocate(reused);
    EXPECT_EQ(pool.GetAllocatedCount(), 0u);
    EXPECT_EQ(pool.GetFreeCount(), 4u);
}
