/// @file ClientServerTests.cpp
/// @brief Integration tests for full client-server loopback scenarios.
///
/// These tests validate the complete client-server communication path:
/// server creation, client connection, input command submission,
/// authoritative simulation, and state replication back to clients.

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Input/Commands/InputCommand.h"
#include "SagaEngine/Input/Networking/InputPacketHandler.h"
#include "SagaEngine/Input/Networking/ServerInputProcessor.h"
#include "SagaEngine/Input/Networking/InputCommandInbox.h"
#include "SagaEngine/Input/Networking/ClientPrediction.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <mutex>
#include <chrono>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;
using namespace SagaEngine::Input;

// ─── Test components ──────────────────────────────────────────────────────────

struct ServerTransform
{
    float posX = 0.f, posY = 0.f, posZ = 0.f;
};

struct ServerVelocity
{
    float velX = 0.f, velY = 0.f, velZ = 0.f;
};

// ─── Helper ───────────────────────────────────────────────────────────────────

static void RegisterTestComponents()
{
    using CR = ECS::ComponentRegistry;
    if (!CR::Instance().IsRegistered<ServerTransform>())
        CR::Instance().Register<ServerTransform>(5001u, "ServerTransform");
    if (!CR::Instance().IsRegistered<ServerVelocity>())
        CR::Instance().Register<ServerVelocity>(5002u, "ServerVelocity");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class ClientServerIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterTestComponents();
        Profiler::Instance().Clear();
    }
};

// ─── Simulated network transport ──────────────────────────────────────────────

struct SimulatedTransport
{
    struct Frame
    {
        std::vector<uint8_t> data;
        float delayMs = 0.f;
    };

    std::queue<Frame> serverBound;
    std::queue<Frame> clientBound;
    std::mutex mutex;
    float latencyMs = 50.f;

    void SendToServer(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mutex);
        serverBound.push({data, latencyMs});
    }

    void SendToClient(const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(mutex);
        clientBound.push({data, latencyMs});
    }

    std::vector<std::vector<uint8_t>> DrainServer()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::vector<uint8_t>> out;
        while (!serverBound.empty())
        {
            out.push_back(std::move(serverBound.front().data));
            serverBound.pop();
        }
        return out;
    }

    std::vector<std::vector<uint8_t>> DrainClient()
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::vector<uint8_t>> out;
        while (!clientBound.empty())
        {
            out.push_back(std::move(clientBound.front().data));
            clientBound.pop();
        }
        return out;
    }

    void SimulateLatency()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(latencyMs) + 5));
    }
};

// ─── Test: Server authoritative simulation loop ──────────────────────────────

TEST_F(ClientServerIntegrationTest, ServerAuthoritativeLoop)
{
    SAGA_PROFILE_FUNCTION();

    WorldState serverWorld;
    SimulationTick simTick(64u);

    // Create player entity on server
    const auto player = serverWorld.CreateEntity();
    serverWorld.AddComponent<ServerTransform>(player.id, {0.f, 0.f, 0.f});
    serverWorld.AddComponent<ServerVelocity>(player.id, {5.f, 0.f, 0.f});

    // Run 64 ticks (1 second at 64Hz)
    constexpr int kTicks = 64;
    const double fixedDt = simTick.FixedDelta();

    for (int i = 0; i < kTicks; ++i)
    {
        simTick.Advance(fixedDt);

        // Integrate velocity
        auto* transform = serverWorld.GetComponent<ServerTransform>(player.id);
        const auto* velocity = serverWorld.GetComponent<ServerVelocity>(player.id);

        if (transform && velocity)
        {
            transform->posX += velocity->velX * static_cast<float>(fixedDt);
            transform->posY += velocity->velY * static_cast<float>(fixedDt);
            transform->posZ += velocity->velZ * static_cast<float>(fixedDt);
        }
    }

    // Verify position after 1 second
    const auto* finalTransform = serverWorld.GetComponent<ServerTransform>(player.id);
    ASSERT_NE(finalTransform, nullptr);
    EXPECT_NEAR(finalTransform->posX, 5.f, 0.01f); // 5 units/sec * 1 sec
}

// ─── Test: Client input submission to server ──────────────────────────────────

TEST_F(ClientServerIntegrationTest, ClientInputSubmission)
{
    SAGA_PROFILE_FUNCTION();

    SimulatedTransport transport;
    transport.latencyMs = 10.f;

    // InputCommandInbox requires a ConnectionId
    using namespace SagaEngine::Networking;
    ConnectionId testConnectionId = 1;
    InputCommandInbox inbox(testConnectionId);
    ServerInputProcessor processor;

    // Client builds input commands
    constexpr int kCommandCount = 20;
    for (uint32_t seq = 0; seq < kCommandCount; ++seq)
    {
        InputCommand cmd = MakeInputCommand(seq, seq + 1, 0);
        cmd.moveX = FixedFromFloat(1.0f);
        cmd.moveY = FixedFromFloat(0.5f);

        // Serialize and send to server
        const auto* bytes = reinterpret_cast<const uint8_t*>(&cmd);
        std::vector<uint8_t> packet(bytes, bytes + sizeof(InputCommand));
        transport.SendToServer(packet);
    }

    transport.SimulateLatency();

    // Server processes received commands
    auto serverPackets = transport.DrainServer();
    EXPECT_EQ(serverPackets.size(), static_cast<size_t>(kCommandCount));

    for (const auto& packet : serverPackets)
    {
        if (packet.size() >= sizeof(InputCommand))
        {
            const auto* cmd = reinterpret_cast<const InputCommand*>(packet.data());
            inbox.PushReceived(*cmd);
        }
    }

    // Verify inbox has the commands - consume for a specific tick
    auto result = inbox.ConsumeForTick(1);
    EXPECT_TRUE(result.command.has_value());
}

// ─── Test: Server-to-client state replication ─────────────────────────────────

TEST_F(ClientServerIntegrationTest, ServerToClientReplication)
{
    SAGA_PROFILE_FUNCTION();

    SimulatedTransport transport;
    transport.latencyMs = 15.f;

    // Server world
    WorldState serverWorld;
    const auto entity = serverWorld.CreateEntity();
    serverWorld.AddComponent<ServerTransform>(entity.id, {100.f, 50.f, 25.f});

    // Client world (starts empty)
    WorldState clientWorld;

    // Server serializes and sends
    const auto snapshot = serverWorld.Serialize();
    transport.SendToClient(snapshot);

    transport.SimulateLatency();

    // Client receives and applies
    auto clientPackets = transport.DrainClient();
    ASSERT_FALSE(clientPackets.empty());

    clientWorld = WorldState::Deserialize(clientPackets.back());

    // Verify client matches server
    EXPECT_EQ(clientWorld.EntityCount(), serverWorld.EntityCount());

    const auto* serverT = serverWorld.GetComponent<ServerTransform>(entity.id);
    const auto* clientT = clientWorld.GetComponent<ServerTransform>(entity.id);

    ASSERT_NE(serverT, nullptr);
    ASSERT_NE(clientT, nullptr);

    EXPECT_FLOAT_EQ(clientT->posX, serverT->posX);
    EXPECT_FLOAT_EQ(clientT->posY, serverT->posY);
    EXPECT_FLOAT_EQ(clientT->posZ, serverT->posZ);
}

// ─── Test: Full round-trip: input -> simulate -> replicate ────────────────────

TEST_F(ClientServerIntegrationTest, FullRoundTrip)
{
    SAGA_PROFILE_FUNCTION();

    SimulatedTransport transport;
    transport.latencyMs = 20.f;

    WorldState serverWorld;
    WorldState clientWorld;

    // Server creates player
    const auto player = serverWorld.CreateEntity();
    serverWorld.AddComponent<ServerTransform>(player.id, {0.f, 0.f, 0.f});
    serverWorld.AddComponent<ServerVelocity>(player.id, {0.f, 0.f, 0.f});

    // Client sends movement input
    InputCommand cmd = MakeInputCommand(1, 1, 0);
    cmd.moveX = FixedFromFloat(1.0f);

    const auto* cmdBytes = reinterpret_cast<const uint8_t*>(&cmd);
    transport.SendToServer(std::vector<uint8_t>(cmdBytes, cmdBytes + sizeof(InputCommand)));

    // Server tick: process input and simulate
    transport.SimulateLatency();
    auto serverPackets = transport.DrainServer();

    if (!serverPackets.empty())
    {
        const auto* receivedCmd = reinterpret_cast<const InputCommand*>(serverPackets.front().data());
        auto* velocity = serverWorld.GetComponent<ServerVelocity>(player.id);
        if (velocity)
        {
            velocity->velX = FloatFromFixed(receivedCmd->moveX);
        }
    }

    // Simulate one tick
    auto* transform = serverWorld.GetComponent<ServerTransform>(player.id);
    const auto* velocity = serverWorld.GetComponent<ServerVelocity>(player.id);
    if (transform && velocity)
    {
        transform->posX += velocity->velX * (1.f / 64.f);
    }

    // Server replicates back to client
    const auto snapshot = serverWorld.Serialize();
    transport.SendToClient(snapshot);

    transport.SimulateLatency();
    auto clientPackets = transport.DrainClient();

    if (!clientPackets.empty())
    {
        clientWorld = WorldState::Deserialize(clientPackets.back());

        const auto* clientT = clientWorld.GetComponent<ServerTransform>(player.id);
        const auto* serverT = serverWorld.GetComponent<ServerTransform>(player.id);

        ASSERT_NE(clientT, nullptr);
        ASSERT_NE(serverT, nullptr);
        EXPECT_FLOAT_EQ(clientT->posX, serverT->posX);
    }
}

// ─── Test: Multiple clients connecting to single server ───────────────────────

TEST_F(ClientServerIntegrationTest, MultipleClientConnections)
{
    SAGA_PROFILE_FUNCTION();

    constexpr int kClientCount = 8;
    WorldState serverWorld;

    // Create entities for each client
    std::vector<ECS::EntityId> players;
    for (int i = 0; i < kClientCount; ++i)
    {
        const auto entity = serverWorld.CreateEntity();
        serverWorld.AddComponent<ServerTransform>(entity.id,
            {static_cast<float>(i * 10), 0.f, 0.f});
        players.push_back(entity.id);
    }

    // Each client gets a snapshot
    const auto snapshot = serverWorld.Serialize();
    std::vector<WorldState> clientWorlds(kClientCount);

    for (int i = 0; i < kClientCount; ++i)
    {
        clientWorlds[static_cast<size_t>(i)] = WorldState::Deserialize(snapshot);
    }

    // Verify all clients have consistent state
    for (int i = 0; i < kClientCount; ++i)
    {
        EXPECT_EQ(clientWorlds[static_cast<size_t>(i)].EntityCount(),
                  serverWorld.EntityCount());

        for (const auto& pid : players)
        {
            const auto* serverT = serverWorld.GetComponent<ServerTransform>(pid);
            const auto* clientT = clientWorlds[static_cast<size_t>(i)].GetComponent<ServerTransform>(pid);

            if (serverT && clientT)
            {
                EXPECT_FLOAT_EQ(clientT->posX, serverT->posX);
            }
        }
    }
}

// ─── Test: Client prediction with server reconciliation ───────────────────────

TEST_F(ClientServerIntegrationTest, PredictionWithReconciliation)
{
    SAGA_PROFILE_FUNCTION();

    struct PredictedState
    {
        float x = 0.f;
        bool operator==(const PredictedState& o) const
        {
            return std::abs(x - o.x) < 0.001f;
        }
    };

    auto simulateFn = [](const PredictedState& state, const InputCommand& cmd)
    {
        PredictedState s = state;
        s.x += FloatFromFixed(cmd.moveX) * 5.f * 0.015625f;
        return s;
    };

    auto equalFn = [](const PredictedState& a, const PredictedState& b)
    {
        return a == b;
    };

    ClientPrediction<PredictedState> prediction({
        .simulate = simulateFn,
        .statesEqual = equalFn,
        .reconcileThresholdSq = 0.01f
    });

    PredictedState currentState{};
    std::vector<InputCommand> commandHistory;

    // 30 ticks of prediction
    for (uint32_t tick = 1; tick <= 30; ++tick)
    {
        InputCommand cmd = MakeInputCommand(tick, tick, 0);
        cmd.moveX = FixedFromFloat(1.0f);

        const auto stateBefore = currentState;
        currentState = simulateFn(currentState, cmd);
        prediction.ApplyLocalCommand(cmd, stateBefore);
        commandHistory.push_back(cmd);
    }

    // Server confirms tick 15 with matching state
    const auto& snapshot = prediction.GetSnapshotAt(15);
    ASSERT_TRUE(snapshot.has_value());

    const auto result = prediction.Reconcile(
        15u, snapshot.value(),
        std::vector<InputCommand>(commandHistory.begin() + 15, commandHistory.end()));

    // No divergence since we use the same simulation
    EXPECT_FALSE(result.corrected);
    EXPECT_GT(prediction.GetPredictedState().x, 0.f);
}

// ─── Test: Server input processing with command ordering ──────────────────────

TEST_F(ClientServerIntegrationTest, InputCommandOrdering)
{
    SAGA_PROFILE_FUNCTION();

    using namespace SagaEngine::Networking;
    ConnectionId testConnectionId = 1;
    InputCommandInbox inbox(testConnectionId);

    // Submit commands out of order
    InputCommand cmd3 = MakeInputCommand(3, 3, 0);
    InputCommand cmd1 = MakeInputCommand(1, 1, 0);
    InputCommand cmd2 = MakeInputCommand(2, 2, 0);

    inbox.PushReceived(cmd3);
    inbox.PushReceived(cmd1);
    inbox.PushReceived(cmd2);

    // Consume in tick order - tick 1
    auto result1 = inbox.ConsumeForTick(1);
    EXPECT_TRUE(result1.command.has_value());
    EXPECT_EQ(result1.command->simulationTick, 1u);

    // Tick 2
    auto result2 = inbox.ConsumeForTick(2);
    EXPECT_TRUE(result2.command.has_value());
    EXPECT_EQ(result2.command->simulationTick, 2u);
}

// ─── Test: Network latency impact on simulation ───────────────────────────────

TEST_F(ClientServerIntegrationTest, LatencyImpactSimulation)
{
    SAGA_PROFILE_FUNCTION();

    SimulatedTransport lowLatency;
    lowLatency.latencyMs = 5.f;

    SimulatedTransport highLatency;
    highLatency.latencyMs = 200.f;

    // Both transport the same command
    InputCommand cmd = MakeInputCommand(1, 1, 0);
    cmd.moveX = FixedFromFloat(1.0f);

    const auto* cmdBytes = reinterpret_cast<const uint8_t*>(&cmd);
    std::vector<uint8_t> packet(cmdBytes, cmdBytes + sizeof(InputCommand));

    lowLatency.SendToServer(packet);
    highLatency.SendToServer(packet);

    // Low latency arrives quickly
    lowLatency.SimulateLatency();
    auto lowPackets = lowLatency.DrainServer();
    EXPECT_FALSE(lowPackets.empty());

    // High latency needs more time
    highLatency.SimulateLatency();
    auto highPackets = highLatency.DrainServer();
    EXPECT_FALSE(highPackets.empty());
}
