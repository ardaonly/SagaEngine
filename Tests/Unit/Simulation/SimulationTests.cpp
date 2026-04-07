/// @file SimulationTests.cpp
/// @brief Unit tests for WorldState, SimulationTick, Deterministic, and Authority.

#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Simulation/Deterministic.h"
#include "SagaEngine/Simulation/Authority.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <thread>
#include <type_traits>
#include <vector>

// ─── Test component types ─────────────────────────────────────────────────────

struct PositionComponent
{
    float x{0.f};
    float y{0.f};
    float z{0.f};
};
static_assert(std::is_trivially_copyable_v<PositionComponent>);

struct HealthComponent
{
    int32_t current{100};
    int32_t max{100};
};
static_assert(std::is_trivially_copyable_v<HealthComponent>);

struct VelocityComponent
{
    float vx{0.f};
    float vy{0.f};
};
static_assert(std::is_trivially_copyable_v<VelocityComponent>);

// ─── Test fixture: registers components once, resets registry after suite ────

class SimulationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
        if (!reg.IsRegistered<PositionComponent>())
            reg.Register<PositionComponent>(1, "PositionComponent");
        if (!reg.IsRegistered<HealthComponent>())
            reg.Register<HealthComponent>(2, "HealthComponent");
        if (!reg.IsRegistered<VelocityComponent>())
            reg.Register<VelocityComponent>(3, "VelocityComponent");
    }
};

// ─── ComponentRegistry ────────────────────────────────────────────────────────

TEST_F(SimulationTest, Registry_IdIsStableAcrossLookups)
{
    auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
    EXPECT_EQ(reg.GetId<PositionComponent>(), 1u);
    EXPECT_EQ(reg.GetId<PositionComponent>(), 1u);
    EXPECT_EQ(reg.GetId<HealthComponent>(),   2u);
    EXPECT_EQ(reg.GetId<VelocityComponent>(), 3u);
}

TEST_F(SimulationTest, Registry_CollisionThrows)
{
    auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
    struct AnotherType { int x; };
    EXPECT_THROW(reg.Register<AnotherType>(1, "AnotherType"), std::logic_error);
}

TEST_F(SimulationTest, Registry_UnregisteredTypeThrows)
{
    struct NeverRegistered { float q; };
    auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
    EXPECT_THROW(reg.GetId<NeverRegistered>(), std::logic_error);
}

TEST_F(SimulationTest, Registry_IsRegisteredPredicate)
{
    struct AlsoNeverRegistered { double d; };
    auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
    EXPECT_TRUE(reg.IsRegistered<PositionComponent>());
    EXPECT_FALSE(reg.IsRegistered<AlsoNeverRegistered>());
}

TEST_F(SimulationTest, Registry_NamesAreReturned)
{
    auto& reg = SagaEngine::ECS::ComponentRegistry::Instance();
    EXPECT_EQ(reg.GetName(1), "PositionComponent");
    EXPECT_EQ(reg.GetName(2), "HealthComponent");
    EXPECT_EQ(reg.GetName(9999), "<unknown>");
}

// ─── WorldState – entity lifecycle ────────────────────────────────────────────

TEST_F(SimulationTest, WorldState_CreateAndIsAlive)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();
    EXPECT_TRUE(handle.IsValid());
    EXPECT_TRUE(world.IsAlive(handle.id));
    EXPECT_EQ(world.EntityCount(), 1u);
}

TEST_F(SimulationTest, WorldState_DestroyEntityRemovesIt)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();
    world.DestroyEntity(handle.id);
    EXPECT_FALSE(world.IsAlive(handle.id));
    EXPECT_FALSE(world.IsValid(handle));
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST_F(SimulationTest, WorldState_DestroyedIdGetsRecycledWithNewVersion)
{
    SagaEngine::Simulation::WorldState world;
    const auto h1 = world.CreateEntity();
    world.DestroyEntity(h1.id);

    const auto h2 = world.CreateEntity();
    EXPECT_EQ(h1.id, h2.id);
    EXPECT_NE(h1.version, h2.version);
    EXPECT_FALSE(world.IsValid(h1));
    EXPECT_TRUE(world.IsValid(h2));
}

TEST_F(SimulationTest, WorldState_DestroyNonAliveEntityIsNoop)
{
    SagaEngine::Simulation::WorldState world;
    EXPECT_NO_FATAL_FAILURE(world.DestroyEntity(9999u));
}

// ─── WorldState – component CRUD ──────────────────────────────────────────────

TEST_F(SimulationTest, WorldState_AddAndGetComponent)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();

    PositionComponent pos{1.f, 2.f, 3.f};
    world.AddComponent<PositionComponent>(handle.id, pos);

    const auto* stored = world.GetComponent<PositionComponent>(handle.id);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->x, 1.f);
    EXPECT_FLOAT_EQ(stored->y, 2.f);
    EXPECT_FLOAT_EQ(stored->z, 3.f);
}

TEST_F(SimulationTest, WorldState_AddOverwritesExistingComponent)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();

    world.AddComponent<PositionComponent>(handle.id, {1.f, 2.f, 3.f});
    world.AddComponent<PositionComponent>(handle.id, {9.f, 8.f, 7.f});

    const auto* stored = world.GetComponent<PositionComponent>(handle.id);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->x, 9.f);
}

TEST_F(SimulationTest, WorldState_GetComponentReturnsNullForMissing)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();
    EXPECT_EQ(world.GetComponent<HealthComponent>(handle.id), nullptr);
}

TEST_F(SimulationTest, WorldState_HasComponent)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();

    EXPECT_FALSE(world.HasComponent<HealthComponent>(handle.id));
    world.AddComponent<HealthComponent>(handle.id, {80, 100});
    EXPECT_TRUE(world.HasComponent<HealthComponent>(handle.id));
}

TEST_F(SimulationTest, WorldState_RemoveComponent)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();

    world.AddComponent<HealthComponent>(handle.id, {80, 100});
    world.RemoveComponent<HealthComponent>(handle.id);
    EXPECT_FALSE(world.HasComponent<HealthComponent>(handle.id));
}

TEST_F(SimulationTest, WorldState_RemoveComponentSwapAndPopPreservesOthers)
{
    SagaEngine::Simulation::WorldState world;
    const auto h0 = world.CreateEntity();
    const auto h1 = world.CreateEntity();
    const auto h2 = world.CreateEntity();

    world.AddComponent<HealthComponent>(h0.id, {10, 10});
    world.AddComponent<HealthComponent>(h1.id, {20, 20});
    world.AddComponent<HealthComponent>(h2.id, {30, 30});

    world.RemoveComponent<HealthComponent>(h1.id);

    ASSERT_NE(world.GetComponent<HealthComponent>(h0.id), nullptr);
    EXPECT_EQ(world.GetComponent<HealthComponent>(h0.id)->current, 10);
    ASSERT_NE(world.GetComponent<HealthComponent>(h2.id), nullptr);
    EXPECT_EQ(world.GetComponent<HealthComponent>(h2.id)->current, 30);
    EXPECT_EQ(world.GetComponent<HealthComponent>(h1.id), nullptr);
}

TEST_F(SimulationTest, WorldState_DestroyEntityRemovesAllComponents)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();

    world.AddComponent<PositionComponent>(handle.id, {});
    world.AddComponent<HealthComponent>(handle.id, {});
    world.DestroyEntity(handle.id);

    EXPECT_EQ(world.TotalComponents(), 0u);
}

TEST_F(SimulationTest, WorldState_AddComponentOnDeadEntityThrows)
{
    SagaEngine::Simulation::WorldState world;
    EXPECT_THROW(
        world.AddComponent<PositionComponent>(9999u, {}),
        std::logic_error);
}

// ─── WorldState – query ───────────────────────────────────────────────────────

TEST_F(SimulationTest, WorldState_QueryReturnsEntitiesWithAllComponents)
{
    SagaEngine::Simulation::WorldState world;

    const auto h0 = world.CreateEntity();
    const auto h1 = world.CreateEntity();
    const auto h2 = world.CreateEntity();

    world.AddComponent<PositionComponent>(h0.id, {});
    world.AddComponent<HealthComponent>(h0.id, {});

    world.AddComponent<PositionComponent>(h1.id, {});
    world.AddComponent<PositionComponent>(h2.id, {});
    world.AddComponent<HealthComponent>(h2.id, {});

    using SagaEngine::ECS::ComponentRegistry;
    const auto posId    = ComponentRegistry::Instance().GetId<PositionComponent>();
    const auto healthId = ComponentRegistry::Instance().GetId<HealthComponent>();

    const auto result = world.Query({posId, healthId});
    ASSERT_EQ(result.size(), 2u);
    EXPECT_LT(result[0], result[1]);
    EXPECT_TRUE(std::find(result.begin(), result.end(), h0.id) != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), h2.id) != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), h1.id) == result.end());
}

TEST_F(SimulationTest, WorldState_QueryWithMissingTypeReturnsEmpty)
{
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();
    world.AddComponent<PositionComponent>(handle.id, {});

    const auto velId = SagaEngine::ECS::ComponentRegistry::Instance().GetId<VelocityComponent>();
    EXPECT_TRUE(world.Query({velId}).empty());
}

// ─── WorldState – serialization ───────────────────────────────────────────────

TEST_F(SimulationTest, WorldState_SerializeDeserializeRoundtrip)
{
    SagaEngine::Simulation::WorldState original;

    const auto h0 = original.CreateEntity();
    const auto h1 = original.CreateEntity();

    original.AddComponent<PositionComponent>(h0.id, {1.f, 2.f, 3.f});
    original.AddComponent<HealthComponent>(h0.id, {75, 100});
    original.AddComponent<PositionComponent>(h1.id, {4.f, 5.f, 6.f});

    const std::vector<uint8_t> bytes = original.Serialize();
    EXPECT_FALSE(bytes.empty());

    const SagaEngine::Simulation::WorldState restored =
        SagaEngine::Simulation::WorldState::Deserialize(bytes);

    EXPECT_EQ(restored.EntityCount(), 2u);
    EXPECT_TRUE(restored.IsAlive(h0.id));
    EXPECT_TRUE(restored.IsAlive(h1.id));

    const auto* pos0 = restored.GetComponent<PositionComponent>(h0.id);
    ASSERT_NE(pos0, nullptr);
    EXPECT_FLOAT_EQ(pos0->x, 1.f);
    EXPECT_FLOAT_EQ(pos0->y, 2.f);
    EXPECT_FLOAT_EQ(pos0->z, 3.f);

    const auto* hp0 = restored.GetComponent<HealthComponent>(h0.id);
    ASSERT_NE(hp0, nullptr);
    EXPECT_EQ(hp0->current, 75);
    EXPECT_EQ(hp0->max, 100);
}

TEST_F(SimulationTest, WorldState_SerializationIsDeterministic)
{
    SagaEngine::Simulation::WorldState world;
    for (int i = 0; i < 5; ++i)
    {
        const auto h = world.CreateEntity();
        world.AddComponent<PositionComponent>(h.id, {static_cast<float>(i), 0.f, 0.f});
    }

    const auto bytes1 = world.Serialize();
    const auto bytes2 = world.Serialize();
    EXPECT_EQ(bytes1, bytes2);
}

TEST_F(SimulationTest, WorldState_DeserializeMalformedInputThrows)
{
    const std::vector<uint8_t> garbage = {0x00, 0x11, 0x22};
    EXPECT_THROW(
        SagaEngine::Simulation::WorldState::Deserialize(garbage),
        std::runtime_error);
}

// ─── WorldState – hashing ─────────────────────────────────────────────────────

TEST_F(SimulationTest, WorldState_EqualStatesProduceSameHash)
{
    SagaEngine::Simulation::WorldState a;
    SagaEngine::Simulation::WorldState b;

    for (int i = 0; i < 3; ++i)
    {
        const auto ha = a.CreateEntity();
        const auto hb = b.CreateEntity();
        a.AddComponent<PositionComponent>(ha.id, {static_cast<float>(i), 0.f, 0.f});
        b.AddComponent<PositionComponent>(hb.id, {static_cast<float>(i), 0.f, 0.f});
    }

    EXPECT_EQ(a.Hash(), b.Hash());
}

TEST_F(SimulationTest, WorldState_DifferentStatesProduceDifferentHashes)
{
    SagaEngine::Simulation::WorldState a;
    SagaEngine::Simulation::WorldState b;

    const auto ha = a.CreateEntity();
    const auto hb = b.CreateEntity();
    a.AddComponent<PositionComponent>(ha.id, {1.f, 0.f, 0.f});
    b.AddComponent<PositionComponent>(hb.id, {2.f, 0.f, 0.f});

    EXPECT_NE(a.Hash(), b.Hash());
}

// ─── SimulationTick ───────────────────────────────────────────────────────────

TEST(SimulationTickTest, TickRateAndDelta)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    EXPECT_EQ(tick.TickRate(), 64u);
    EXPECT_DOUBLE_EQ(tick.FixedDelta(), 1.0 / 64.0);
}

TEST(SimulationTickTest, AdvanceProducesCorrectTickCount)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    const uint32_t count = tick.Advance(1.0 / 64.0);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(tick.CurrentTick(), 1u);
}

TEST(SimulationTickTest, AdvanceAccumulatesSubTickTime)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    const double halfTick = 0.5 / 64.0;

    EXPECT_EQ(tick.Advance(halfTick), 0u);
    EXPECT_EQ(tick.Advance(halfTick), 1u);
    EXPECT_EQ(tick.CurrentTick(), 1u);
}

TEST(SimulationTickTest, AdvanceClampsSpiralOfDeath)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    const uint32_t ticks = tick.Advance(10.0);
    EXPECT_LE(ticks, 8u);
}

TEST(SimulationTickTest, AdvanceAccumulatesTicksCorrectly)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    uint32_t total = 0;
    for (int i = 0; i < 100; ++i)
        total += tick.Advance(1.0 / 60.0);

    EXPECT_GE(total, 105u);
    EXPECT_LE(total, 108u);
    EXPECT_EQ(tick.CurrentTick(), static_cast<uint64_t>(total));
}

TEST(SimulationTickTest, AlphaIsInUnitRange)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    tick.Advance(0.3 / 64.0);
    EXPECT_GE(tick.Alpha(), 0.f);
    EXPECT_LT(tick.Alpha(), 1.f);
}

TEST(SimulationTickTest, ResetClearsState)
{
    SagaEngine::Simulation::SimulationTick tick(64u);
    tick.Advance(1.0);
    tick.Reset();
    EXPECT_EQ(tick.CurrentTick(), 0u);
    EXPECT_DOUBLE_EQ(tick.Accumulator(), 0.0);
}

// ─── Deterministic ────────────────────────────────────────────────────────────

class DeterministicTest : public SimulationTest {};

TEST_F(DeterministicTest, RecordAndLookupHash)
{
    SagaEngine::Simulation::Deterministic det;
    SagaEngine::Simulation::WorldState world;
    const auto handle = world.CreateEntity();
    world.AddComponent<PositionComponent>(handle.id, {1.f, 2.f, 3.f});

    const uint64_t hash = det.Record(42u, world);
    EXPECT_NE(hash, 0u);

    const auto stored = det.HashAt(42u);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(*stored, hash);

    EXPECT_EQ(det.LatestTick(), 42u);
}

TEST_F(DeterministicTest, VerifyReturnsTrueOnMatch)
{
    SagaEngine::Simulation::Deterministic det;
    SagaEngine::Simulation::WorldState world;
    world.CreateEntity();
    const uint64_t hash = det.Record(10u, world);
    EXPECT_TRUE(det.Verify(10u, hash));
}

TEST_F(DeterministicTest, VerifyReturnsFalseOnMismatch)
{
    SagaEngine::Simulation::Deterministic det;
    SagaEngine::Simulation::WorldState world;
    world.CreateEntity();
    det.Record(10u, world);
    EXPECT_FALSE(det.Verify(10u, 0xDEADBEEFDEADBEEFull));
}

TEST_F(DeterministicTest, VerifyReturnsTrueForOutOfWindowTick)
{
    SagaEngine::Simulation::Deterministic det;
    EXPECT_TRUE(det.Verify(9999u, 0u));
}

TEST_F(DeterministicTest, RingBufferOverwritesOldEntries)
{
    SagaEngine::Simulation::Deterministic det;
    SagaEngine::Simulation::WorldState world;

    det.Record(0u, world);
    const auto h0before = det.HashAt(0u);

    for (uint32_t t = 1; t <= SagaEngine::Simulation::Deterministic::kHistoryDepth; ++t)
        det.Record(t, world);

    const auto h0after = det.HashAt(0u);
    EXPECT_FALSE(h0after.has_value());
}

TEST_F(DeterministicTest, ResetClearsAllHistory)
{
    SagaEngine::Simulation::Deterministic det;
    SagaEngine::Simulation::WorldState world;
    det.Record(5u, world);
    det.Reset();
    EXPECT_FALSE(det.HashAt(5u).has_value());
    EXPECT_EQ(det.LatestTick(), 0u);
}

// ─── Authority ────────────────────────────────────────────────────────────────

class AuthorityTest : public SimulationTest {};

class SpawnPositionSystem final : public SagaEngine::Simulation::ISimulationSystem
{
public:
    const char* Name() const noexcept override { return "SpawnPositionSystem"; }

    void Update(
        SagaEngine::Simulation::WorldState& state,
        std::span<const SagaEngine::Input::ClientTickEntry>,
        uint64_t, double) override
    {
        using SagaEngine::ECS::ComponentRegistry;
        const auto healthId = ComponentRegistry::Instance().GetId<HealthComponent>();
        const auto posId    = ComponentRegistry::Instance().GetId<PositionComponent>();

        const auto needsPos = state.Query({healthId});
        for (SagaEngine::ECS::EntityId eid : needsPos)
        {
            if (!state.HasComponent<PositionComponent>(eid))
                state.AddComponent<PositionComponent>(eid, {0.f, 0.f, 0.f});
        }
        ++ticksRun;
        (void)posId;
    }

    int ticksRun{0};
};

class MoveSystem final : public SagaEngine::Simulation::ISimulationSystem
{
public:
    const char* Name() const noexcept override { return "MoveSystem"; }

    void Update(
        SagaEngine::Simulation::WorldState& state,
        std::span<const SagaEngine::Input::ClientTickEntry>,
        uint64_t, double) override
    {
        using SagaEngine::ECS::ComponentRegistry;
        const auto posId = ComponentRegistry::Instance().GetId<PositionComponent>();
        for (SagaEngine::ECS::EntityId eid : state.Query({posId}))
        {
            auto* pos = state.GetComponent<PositionComponent>(eid);
            if (pos)
                pos->x += 1.f;
        }
    }
};

TEST_F(AuthorityTest, RegisterSystem)
{
    SagaEngine::Simulation::Authority auth;
    auth.RegisterSystem(std::make_unique<MoveSystem>());
    EXPECT_EQ(auth.SystemCount(), 1u);
}

TEST_F(AuthorityTest, TickRunsSystemsInOrder)
{
    SagaEngine::Simulation::Authority auth;

    auto* spawn = new SpawnPositionSystem();
    auth.RegisterSystem(std::unique_ptr<SagaEngine::Simulation::ISimulationSystem>(spawn));
    auth.RegisterSystem(std::make_unique<MoveSystem>());

    const auto handle = auth.GetWorldState().CreateEntity();
    auth.GetWorldState().AddComponent<HealthComponent>(handle.id, {100, 100});

    const auto result = auth.Tick(
        1u,
        1.0 / 64.0,
        std::span<const SagaEngine::Input::ClientTickEntry>{});

    EXPECT_EQ(result.tick, 1u);
    EXPECT_NE(result.stateHash, 0u);
    EXPECT_EQ(spawn->ticksRun, 1);

    const auto* pos = auth.GetWorldState().GetComponent<PositionComponent>(handle.id);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.f);
}

TEST_F(AuthorityTest, TickHashChangesAfterMutation)
{
    SagaEngine::Simulation::Authority auth;
    auth.RegisterSystem(std::make_unique<MoveSystem>());

    const auto handle = auth.GetWorldState().CreateEntity();
    auth.GetWorldState().AddComponent<PositionComponent>(handle.id, {0.f, 0.f, 0.f});

    const auto r1 = auth.Tick(
        1u,
        1.0 / 64.0,
        std::span<const SagaEngine::Input::ClientTickEntry>{});
    const auto r2 = auth.Tick(
        2u,
        1.0 / 64.0,
        std::span<const SagaEngine::Input::ClientTickEntry>{});

    EXPECT_NE(r1.stateHash, r2.stateHash);
}

TEST_F(AuthorityTest, DeterministicIntegration)
{
    SagaEngine::Simulation::Authority auth;
    auth.RegisterSystem(std::make_unique<MoveSystem>());

    const auto handle = auth.GetWorldState().CreateEntity();
    auth.GetWorldState().AddComponent<PositionComponent>(handle.id, {0.f, 0.f, 0.f});

    const auto r = auth.Tick(
        5u,
        1.0 / 64.0,
        std::span<const SagaEngine::Input::ClientTickEntry>{});

    const auto stored = auth.GetDeterministic().HashAt(5u);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(*stored, r.stateHash);
}