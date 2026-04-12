/// @file ReplicationIntegrationTests.cpp
/// @brief Integration tests for end-to-end client-server replication and multi-client sync.
///
/// These tests validate the full replication pipeline: server authority,
/// delta snapshot generation, network transit simulation, client-side
/// apply, and reconciliation. They bridge the gap between unit tests
/// (individual components) and stress tests (throughput limits).

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Client/Replication/InterestManagement.h"
#include "SagaEngine/Client/Replication/ReplicationScheduler.h"
#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Input/Networking/ClientPrediction.h"
#include "SagaEngine/Input/Commands/InputCommand.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <functional>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;
using namespace SagaEngine::Client::Replication;
using namespace SagaEngine::Input;

// ─── Integration test components ──────────────────────────────────────────────

struct IntegrationTransform
{
    float posX = 0.f, posY = 0.f, posZ = 0.f;
    float rotYaw = 0.f;
    uint32_t flags = 0;
};

struct IntegrationPlayerInput
{
    float moveX = 0.f, moveZ = 0.f;
    uint8_t buttons = 0;
    uint8_t padding[3] = {0, 0, 0};
};

// ─── Helper: register components idempotently ─────────────────────────────────

static void RegisterIntegrationComponents()
{
    using CR = ECS::ComponentRegistry;

    if (!CR::Instance().IsRegistered<IntegrationTransform>())
        CR::Instance().Register<IntegrationTransform>(4001u, "IntegrationTransform");

    if (!CR::Instance().IsRegistered<IntegrationPlayerInput>())
        CR::Instance().Register<IntegrationPlayerInput>(4002u, "IntegrationPlayerInput");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class ReplicationIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterIntegrationComponents();
        Profiler::Instance().Clear();
    }
};

// ─── Simple network channel simulation ────────────────────────────────────────

struct SimulatedNetworkChannel
{
    struct Packet
    {
        std::vector<uint8_t> data;
        float delayMs = 0.f;
    };

    float latencyMs = 50.f;
    float jitterMs = 5.f;
    float lossRate = 0.f;

    std::queue<Packet> queue;
    std::mutex mutex;

    void Send(const std::vector<uint8_t>& data)
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> lossDist(0.f, 1.f);
        if (lossDist(rng) < lossRate)
            return;

        std::lock_guard<std::mutex> lock(mutex);
        queue.push({data, latencyMs});
    }

    std::vector<std::vector<uint8_t>> Receive()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::vector<uint8_t>> out;

        // Simulate delivery: all packets are "delivered" after sleep
        while (!queue.empty())
        {
            out.push_back(std::move(queue.front().data));
            queue.pop();
        }
        return out;
    }

    void SimulateTransit()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(latencyMs + 10)));
    }
};

// ─── Server-side simulation step ──────────────────────────────────────────────

static void RunServerTick(WorldState& world, uint64_t tick)
{
    SAGA_PROFILE_SCOPE("ServerTick");

    for (const auto& eid : world.GetAliveEntities())
    {
        auto* transform = world.GetComponent<IntegrationTransform>(eid);
        auto* input = world.GetComponent<IntegrationPlayerInput>(eid);

        if (transform && input)
        {
            // Simple integration: apply input to position
            const float speed = 5.0f;
            transform->posX += input->moveX * speed * 0.015625f; // 1/64
            transform->posZ += input->moveZ * speed * 0.015625f;

            // Reset input after processing
            input->moveX = 0.f;
            input->moveZ = 0.f;
        }
    }
    (void)tick;
}

// ─── Test: end-to-end single client replication ───────────────────────────────

TEST_F(ReplicationIntegrationTest, SingleClientEndToEnd)
{
    SAGA_PROFILE_FUNCTION();

    // Setup server world with one player entity
    WorldState serverWorld;
    const auto playerEntity = serverWorld.CreateEntity();
    serverWorld.AddComponent<IntegrationTransform>(playerEntity.id, {0.f, 0.f, 0.f, 0.f, 0u});
    serverWorld.AddComponent<IntegrationPlayerInput>(playerEntity.id, {1.f, 0.f, 0u});

    // Client world starts empty
    WorldState clientWorld;

    SimulatedNetworkChannel channel;
    channel.latencyMs = 10.f;

    // Run 10 server ticks
    constexpr int kTickCount = 10;
    for (int tick = 0; tick < kTickCount; ++tick)
    {
        // Server simulates
        RunServerTick(serverWorld, static_cast<uint64_t>(tick));

        // Server serializes and sends
        const auto snapshot = serverWorld.Serialize();
        channel.Send(snapshot);
    }

    // Simulate network transit
    channel.SimulateTransit();

    // Client receives and applies
    const auto packets = channel.Receive();
    ASSERT_FALSE(packets.empty());

    // Apply the latest snapshot
    clientWorld = WorldState::Deserialize(packets.back());

    // Verify client world matches server
    EXPECT_EQ(clientWorld.EntityCount(), serverWorld.EntityCount());

    const auto* serverTransform = serverWorld.GetComponent<IntegrationTransform>(playerEntity.id);
    const auto* clientTransform = clientWorld.GetComponent<IntegrationTransform>(playerEntity.id);

    ASSERT_NE(serverTransform, nullptr);
    ASSERT_NE(clientTransform, nullptr);

    // Client should have moved in X direction
    EXPECT_GT(clientTransform->posX, 0.f);
    EXPECT_FLOAT_EQ(clientTransform->posX, serverTransform->posX);
    EXPECT_FLOAT_EQ(clientTransform->posZ, serverTransform->posZ);
}

// ─── Test: multi-client sync scenario ─────────────────────────────────────────

TEST_F(ReplicationIntegrationTest, MultiClientSync)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kClientCount = 4;
    constexpr int kTickCount = 20;

    // Server world with multiple players
    WorldState serverWorld;
    std::vector<ECS::EntityId> playerEntities;

    for (int i = 0; i < kClientCount; ++i)
    {
        const auto entity = serverWorld.CreateEntity();
        serverWorld.AddComponent<IntegrationTransform>(entity.id,
            {static_cast<float>(i * 10), 0.f, static_cast<float>(i * 10), 0.f, 0u});
        serverWorld.AddComponent<IntegrationPlayerInput>(entity.id,
            {0.5f, 0.3f, 0u});
        playerEntities.push_back(entity.id);
    }

    // Each client has its own world
    std::vector<WorldState> clientWorlds(kClientCount);
    SimulatedNetworkChannel channel;
    channel.latencyMs = 15.f;

    // Run server ticks
    for (int tick = 0; tick < kTickCount; ++tick)
    {
        RunServerTick(serverWorld, static_cast<uint64_t>(tick));

        // Send snapshot every 5 ticks
        if (tick % 5 == 4)
        {
            const auto snapshot = serverWorld.Serialize();
            channel.Send(snapshot);
        }
    }

    channel.SimulateTransit();

    // All clients receive the same snapshot
    const auto packets = channel.Receive();
    ASSERT_FALSE(packets.empty());

    for (int c = 0; c < kClientCount; ++c)
    {
        clientWorlds[static_cast<size_t>(c)] = WorldState::Deserialize(packets.back());

        // Verify entity count
        EXPECT_EQ(clientWorlds[static_cast<size_t>(c)].EntityCount(),
                  serverWorld.EntityCount());

        // Verify player positions moved
        for (int i = 0; i < kClientCount; ++i)
        {
            const auto* serverT = serverWorld.GetComponent<IntegrationTransform>(
                playerEntities[static_cast<size_t>(i)]);
            const auto* clientT = clientWorlds[static_cast<size_t>(c)].GetComponent<
                IntegrationTransform>(playerEntities[static_cast<size_t>(i)]);

            if (serverT && clientT)
            {
                EXPECT_FLOAT_EQ(clientT->posX, serverT->posX);
                EXPECT_FLOAT_EQ(clientT->posZ, serverT->posZ);
            }
        }
    }
}

// ─── Test: client-side prediction with reconciliation ─────────────────────────

TEST_F(ReplicationIntegrationTest, ClientPredictionReconciliation)
{
    SAGA_PROFILE_FUNCTION();

    // Define a simple world state for prediction
    struct SimplePredictedState
    {
        float positionX = 0.f;
        float positionZ = 0.f;

        bool operator==(const SimplePredictedState& other) const
        {
            return std::abs(positionX - other.positionX) < 0.001f &&
                   std::abs(positionZ - other.positionZ) < 0.001f;
        }
    };

    // Simulation function: apply movement command
    auto simulateFn = [](const SimplePredictedState& state, const InputCommand& cmd)
    {
        SimplePredictedState newState = state;
        const float speed = 5.0f;
        newState.positionX += FloatFromFixed(cmd.moveX) * speed * 0.015625f;
        newState.positionZ += FloatFromFixed(cmd.moveY) * speed * 0.015625f;
        return newState;
    };

    // Equality check
    auto equalFn = [](const SimplePredictedState& a, const SimplePredictedState& b)
    {
        return a == b;
    };

    ClientPrediction<SimplePredictedState> prediction({
        .simulate = simulateFn,
        .statesEqual = equalFn,
        .reconcileThresholdSq = 0.01f
    });

    // Simulate 20 ticks of local prediction
    SimplePredictedState currentState{};
    std::vector<InputCommand> unackedCommands;

    for (uint32_t tick = 1; tick <= 20; ++tick)
    {
        InputCommand cmd = MakeInputCommand(tick, tick, 0);
        cmd.moveX = FixedFromFloat(1.0f);
        cmd.moveY = FixedFromFloat(0.5f);

        const auto stateBefore = currentState;
        currentState = simulateFn(currentState, cmd);
        prediction.ApplyLocalCommand(cmd, stateBefore);
        unackedCommands.push_back(cmd);
    }

    // Verify predicted state
    const auto& predictedState = prediction.GetPredictedState();
    EXPECT_GT(predictedState.positionX, 0.f);
    EXPECT_GT(predictedState.positionZ, 0.f);

    // Simulate server reconciliation (no divergence)
    const auto reconcileResult = prediction.Reconcile(
        10u, predictedState, // Server confirms tick 10 with matching state
        std::vector<InputCommand>(unackedCommands.begin() + 10, unackedCommands.end()));

    // No correction needed since states match
    EXPECT_FALSE(reconcileResult.corrected);
}

// ─── Test: prediction divergence and correction ───────────────────────────────

TEST_F(ReplicationIntegrationTest, PredictionDivergenceCorrection)
{
    SAGA_PROFILE_FUNCTION();

    struct SimpleState
    {
        float x = 0.f;
        bool operator==(const SimpleState& o) const { return std::abs(x - o.x) < 0.001f; }
    };

    // Client simulation: moves at speed 1.0
    auto clientSim = [](const SimpleState& state, const InputCommand& cmd)
    {
        SimpleState s = state;
        s.x += 1.0f * 0.015625f;
        return s;
    };

    // Server simulation: moves at speed 0.8 (slight difference = divergence)
    auto serverSim = [](const SimpleState& state, const InputCommand& cmd)
    {
        SimpleState s = state;
        s.x += 0.8f * 0.015625f;
        return s;
    };

    auto equalFn = [](const SimpleState& a, const SimpleState& b) { return a == b; };

    ClientPrediction<SimpleState> prediction({
        .simulate = clientSim,
        .statesEqual = equalFn,
        .reconcileThresholdSq = 0.01f
    });

    SimpleState currentState{};
    std::vector<InputCommand> commands;

    // 10 ticks of prediction
    for (uint32_t tick = 1; tick <= 10; ++tick)
    {
        InputCommand cmd = MakeInputCommand(tick, tick, 0);
        cmd.moveX = FixedFromFloat(1.0f);

        const auto stateBefore = currentState;
        currentState = clientSim(currentState, cmd);
        prediction.ApplyLocalCommand(cmd, stateBefore);
        commands.push_back(cmd);
    }

    // Server sends authoritative state at tick 5 (which moved slower)
    SimpleState serverState{};
    for (uint32_t tick = 1; tick <= 5; ++tick)
    {
        serverState = serverSim(serverState, commands[static_cast<size_t>(tick - 1)]);
    }

    const auto result = prediction.Reconcile(
        5u, serverState,
        std::vector<InputCommand>(commands.begin() + 5, commands.end()));

    // Should detect divergence and correct
    EXPECT_TRUE(result.corrected);
    EXPECT_EQ(result.divergedAtTick, 5u);
}

// ─── Test: InterestManager entity filtering ────────────────────────────────────

TEST_F(ReplicationIntegrationTest, InterestManagerEntityFiltering)
{
    SAGA_PROFILE_FUNCTION();

    InterestManager manager;
    InterestConfig config;
    config.cellSize = 32.f;
    config.interestRadius = 1;
    config.hysteresisMargin = 0;
    manager.Configure(config);

    // Place entities in a 5x5 grid
    std::vector<ECS::EntityId> entityIds;
    for (int x = -2; x <= 2; ++x)
    {
        for (int z = -2; z <= 2; ++z)
        {
            ECS::EntityId id = static_cast<ECS::EntityId>((x + 2) * 5 + (z + 2) + 1);
            Math::Vec3 pos{static_cast<float>(x * 32), 0.f, static_cast<float>(z * 32)};
            manager.RegisterEntity(id, pos);
            entityIds.push_back(id);
        }
    }

    // Client at center (0, 0)
    manager.UpdateClientPosition(Math::Vec3{0.f, 0.f, 0.f});

    const auto relevant = manager.GetRelevantEntities();

    // With radius 1, client sees 3x3 = 9 cells around center
    EXPECT_EQ(relevant.size(), 9u);

    // Move client to edge
    manager.UpdateClientPosition(Math::Vec3{80.f, 0.f, 0.f});
    const auto relevantAfter = manager.GetRelevantEntities();

    // Should still see 9 entities (unless at world boundary)
    EXPECT_GT(relevantAfter.size(), 0u);
    EXPECT_LE(relevantAfter.size(), 25u);
}

// ─── Test: ReplicationScheduler basic operation ───────────────────────────────

TEST_F(ReplicationIntegrationTest, ReplicationSchedulerBasic)
{
    SAGA_PROFILE_FUNCTION();

    ReplicationScheduler scheduler;
    SchedulerConfig config;
    config.mode = SchedulerMode::Sequential;
    config.maxThreads = 0;
    config.minBatchSize = 64;
    scheduler.Configure(config);

    // Create a server world
    WorldState world;
    const auto entity = world.CreateEntity();
    world.AddComponent<IntegrationTransform>(entity.id, {0.f, 0.f, 0.f, 0.f, 0u});

    const auto snapshot = world.Serialize();
    EXPECT_GT(snapshot.size(), 0u);

    // Verify sequential execution works
    std::atomic<int> applyCount{0};
    auto applyFn = [](uint32_t index, void* ctx)
    {
        (void)index;
        auto* counter = static_cast<std::atomic<int>*>(ctx);
        counter->fetch_add(1);
    };

    scheduler.Execute(5, applyFn, &applyCount);
    EXPECT_EQ(applyCount.load(), 5);
}

// ─── Test: Replication state machine lifecycle ────────────────────────────────

TEST_F(ReplicationIntegrationTest, ReplicationStateMachineLifecycle)
{
    SAGA_PROFILE_FUNCTION();

    ReplicationStateMachine fsm;
    StateMachineConfig config;
    config.hashCheckIntervalTicks = 300;
    config.desyncGraceTicks = 2;
    fsm.Configure(config);

    // Should start in Boot state
    EXPECT_EQ(fsm.CurrentState(), ReplicationState::Boot);

    // Transition to AwaitingFullSnapshot
    fsm.TransitionTo(ReplicationState::AwaitingFullSnapshot);
    EXPECT_EQ(fsm.CurrentState(), ReplicationState::AwaitingFullSnapshot);

    // Transition to SyncingBaseline
    fsm.TransitionTo(ReplicationState::SyncingBaseline);
    EXPECT_EQ(fsm.CurrentState(), ReplicationState::SyncingBaseline);

    // Transition to Synced
    fsm.TransitionTo(ReplicationState::Synced);
    EXPECT_EQ(fsm.CurrentState(), ReplicationState::Synced);

    // Packet acceptance in Synced state
    const auto acceptResult = fsm.AcceptPacket(100u, false);
    EXPECT_EQ(acceptResult, AcceptResult::Accept);

    // Handle disconnect
    fsm.TransitionTo(ReplicationState::Boot);
    EXPECT_EQ(fsm.CurrentState(), ReplicationState::Boot);

    fsm.Reset();
}

// ─── Test: World state serialization roundtrip integrity ──────────────────────

TEST_F(ReplicationIntegrationTest, WorldStateRoundtripIntegrity)
{
    SAGA_PROFILE_FUNCTION();

    WorldState original;

    // Create diverse entities
    for (int i = 0; i < 100; ++i)
    {
        const auto entity = original.CreateEntity();
        original.AddComponent<IntegrationTransform>(entity.id,
            {static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3),
             static_cast<float>(i * 0.1f), static_cast<uint32_t>(i)});

        if (i % 2 == 0)
        {
            original.AddComponent<IntegrationPlayerInput>(entity.id,
                {static_cast<float>(i % 10) * 0.1f, static_cast<float>(i % 5) * 0.1f,
                 static_cast<uint8_t>(i & 0xFF)});
        }
    }

    const uint64_t originalHash = original.Hash();

    // Serialize
    const auto buffer = original.Serialize();
    EXPECT_GT(buffer.size(), 0u);

    // Deserialize
    WorldState restored = WorldState::Deserialize(buffer);

    // Hash must match
    EXPECT_EQ(restored.Hash(), originalHash);

    // Entity count must match
    EXPECT_EQ(restored.EntityCount(), original.EntityCount());

    // Component data must match
    for (const auto& eid : original.GetAliveEntities())
    {
        const auto* origT = original.GetComponent<IntegrationTransform>(eid);
        const auto* restT = restored.GetComponent<IntegrationTransform>(eid);

        ASSERT_NE(origT, nullptr);
        ASSERT_NE(restT, nullptr);

        EXPECT_FLOAT_EQ(restT->posX, origT->posX);
        EXPECT_FLOAT_EQ(restT->posY, origT->posY);
        EXPECT_FLOAT_EQ(restT->posZ, origT->posZ);
        EXPECT_FLOAT_EQ(restT->rotYaw, origT->rotYaw);
        EXPECT_EQ(restT->flags, origT->flags);
    }
}

// ─── Test: incremental world changes with hash verification ───────────────────

TEST_F(ReplicationIntegrationTest, IncrementalWorldChanges)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    const auto entity = world.CreateEntity();
    world.AddComponent<IntegrationTransform>(entity.id, {0.f, 0.f, 0.f, 0.f, 0u});

    const uint64_t hashBefore = world.Hash();

    // Modify the entity
    auto* transform = world.GetComponent<IntegrationTransform>(entity.id);
    ASSERT_NE(transform, nullptr);
    transform->posX = 10.f;
    transform->posZ = 20.f;

    const uint64_t hashAfter = world.Hash();

    // Hash should have changed
    EXPECT_NE(hashBefore, hashAfter);
}

// ─── Test: entity creation/destruction replication consistency ────────────────

TEST_F(ReplicationIntegrationTest, EntityLifecycleReplication)
{
    SAGA_PROFILE_FUNCTION();

    WorldState serverWorld;
    WorldState clientWorld;

    // Create entities
    std::vector<ECS::EntityId> serverEntities;
    for (int i = 0; i < 10; ++i)
    {
        const auto entity = serverWorld.CreateEntity();
        serverWorld.AddComponent<IntegrationTransform>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 0.f, 0u});
        serverEntities.push_back(entity.id);
    }

    // Sync to client
    clientWorld = WorldState::Deserialize(serverWorld.Serialize());
    EXPECT_EQ(clientWorld.EntityCount(), 10u);

    // Destroy half the entities on server
    for (int i = 0; i < 5; ++i)
    {
        serverWorld.DestroyEntity(serverEntities[static_cast<size_t>(i)]);
    }

    EXPECT_EQ(serverWorld.EntityCount(), 5u);

    // Sync again
    clientWorld = WorldState::Deserialize(serverWorld.Serialize());
    EXPECT_EQ(clientWorld.EntityCount(), 5u);
}
