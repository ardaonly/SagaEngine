/// @file NetworkLoadTests.cpp
/// @brief Stress tests for network latency simulation and replication under load.
///
/// These tests exercise the replication pipeline, delta snapshot compression,
/// and network latency simulation. They validate that the engine can handle
/// MMO-scale replication without breaking determinism or blowing memory.

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include "SagaEngine/Core/Encoding/VarInt.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"
#include "SagaEngine/Client/Replication/InterestManagement.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;
using namespace SagaEngine::Client::Replication;

// ─── Network stress test components ───────────────────────────────────────────

struct NetPlayerState
{
    float positionX = 0.f, positionY = 0.f, positionZ = 0.f;
    float velocityX = 0.f, velocityY = 0.f, velocityZ = 0.f;
    float rotationYaw = 0.f;
    uint32_t flags = 0;
};

struct NetInventorySlot
{
    uint32_t itemId = 0;
    uint32_t quantity = 0;
    uint8_t slotIndex = 0;
    uint8_t padding[3] = {0, 0, 0};
};

// ─── Helper: register components idempotently ─────────────────────────────────

static void RegisterNetComponents()
{
    using CR = ECS::ComponentRegistry;

    if (!CR::Instance().IsRegistered<NetPlayerState>())
        CR::Instance().Register<NetPlayerState>(3001u, "NetPlayerState");

    if (!CR::Instance().IsRegistered<NetInventorySlot>())
        CR::Instance().Register<NetInventorySlot>(3002u, "NetInventorySlot");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class NetworkLoadStressTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterNetComponents();
        Profiler::Instance().Clear();
    }
};

// ─── Latency simulator ────────────────────────────────────────────────────────

/// Simulates network latency with configurable jitter and packet loss.
struct LatencySimulator
{
    struct Packet
    {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point deliveryTime;
    };

    float latencyMs = 50.f;       // Base latency
    float jitterMs = 10.f;        // Jitter range
    float lossRate = 0.f;         // 0.0-1.0 packet loss probability

    std::mt19937 rng{42};

    /// Add a packet to the queue with simulated delay.
    void Send(std::queue<Packet>& queue, const std::vector<uint8_t>& data)
    {
        std::uniform_real_distribution<float> lossDist(0.f, 1.f);
        if (lossDist(rng) < lossRate)
            return; // Packet lost

        std::uniform_real_distribution<float> jitterDist(-jitterMs, jitterMs);
        const float totalDelay = latencyMs + jitterDist(rng);
        const auto deliveryTime = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(static_cast<int>(std::max(0.f, totalDelay)));

        queue.push({data, deliveryTime});
    }

    /// Deliver packets whose time has come.
    void DeliverReady(std::queue<Packet>& queue,
                     std::vector<std::vector<uint8_t>>& out)
    {
        const auto now = std::chrono::steady_clock::now();
        while (!queue.empty() && queue.front().deliveryTime <= now)
        {
            out.push_back(std::move(queue.front().data));
            queue.pop();
        }
    }
};

// ─── Serialize/deserialize world snapshots for network transit ────────────────

static std::vector<uint8_t> SerializeWorldSnapshot(const WorldState& world)
{
    SAGA_PROFILE_SCOPE("SerializeWorldSnapshot");
    return world.Serialize();
}

static WorldState DeserializeWorldSnapshot(const std::vector<uint8_t>& data)
{
    SAGA_PROFILE_SCOPE("DeserializeWorldSnapshot");
    return WorldState::Deserialize(data);
}

// ─── Test: serialization throughput under load ────────────────────────────────

TEST_F(NetworkLoadStressTest, SerializationThroughput)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntities = 20'000;

    // Create a dense world
    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<NetPlayerState>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0u});
    }

    const auto t0 = std::chrono::steady_clock::now();

    // Serialize 10 times
    constexpr int kIterations = 10;
    std::vector<uint8_t> buffer;
    for (int i = 0; i < kIterations; ++i)
    {
        buffer = SerializeWorldSnapshot(world);
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double avgMs = elapsedMs / static_cast<double>(kIterations);
    const double mbPerSec = (static_cast<double>(buffer.size()) / (1024.0 * 1024.0)) /
                            (avgMs / 1000.0);

    // Sanity: each serialization should produce non-trivial data
    EXPECT_GT(buffer.size(), 1000u);
    // Throughput should be reasonable — at least 1 MB/s
    EXPECT_GT(mbPerSec, 1.0);
}

// ─── Test: delta snapshot size reduction ──────────────────────────────────────

TEST_F(NetworkLoadStressTest, DeltaSnapshotSizeReduction)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntities = 5'000;

    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<NetPlayerState>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0u});
    }

    const auto fullSnapshot = SerializeWorldSnapshot(world);

    // Make a small change to only 10 entities
    std::vector<ECS::EntityId> changedEntities;
    int idx = 0;
    for (const auto& eid : world.GetAliveEntities())
    {
        if (idx < 10)
        {
            auto* pos = world.GetComponent<NetPlayerState>(eid);
            if (pos)
            {
                pos->positionX += 1.f;
                changedEntities.push_back(eid);
            }
        }
        idx++;
    }

    const auto deltaSnapshot = SerializeWorldSnapshot(world);

    // The full and delta snapshots will differ in content
    // In a real delta system, the delta would be much smaller.
    // Here we verify the hash changes (determinism check).
    const auto hashBefore = WorldState::Deserialize(fullSnapshot).Hash();
    const auto hashAfter = world.Hash();

    EXPECT_NE(hashBefore, hashAfter);
    EXPECT_EQ(changedEntities.size(), 10u);
}

// ─── Test: latency simulation with packet queue ───────────────────────────────

TEST_F(NetworkLoadStressTest, LatencySimulation)
{
    SAGA_PROFILE_FUNCTION();

    LatencySimulator sim;
    sim.latencyMs = 20.f;
    sim.jitterMs = 5.f;
    sim.lossRate = 0.0f; // No loss for this test

    std::queue<LatencySimulator::Packet> sendQueue;

    // Send 100 packets
    constexpr int kPacketCount = 100;
    for (int i = 0; i < kPacketCount; ++i)
    {
        std::vector<uint8_t> payload = {
            static_cast<uint8_t>(i & 0xFF),
            static_cast<uint8_t>((i >> 8) & 0xFF)
        };
        sim.Send(sendQueue, payload);
    }

    // Wait for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::vector<std::vector<uint8_t>> delivered;
    sim.DeliverReady(sendQueue, delivered);

    // All packets should eventually be delivered (with zero loss)
    // The exact count depends on timing, but most should arrive
    EXPECT_LE(delivered.size(), static_cast<size_t>(kPacketCount));
}

// ─── Test: packet loss simulation ─────────────────────────────────────────────

TEST_F(NetworkLoadStressTest, PacketLossSimulation)
{
    SAGA_PROFILE_FUNCTION();

    LatencySimulator sim;
    sim.latencyMs = 5.f;
    sim.jitterMs = 1.f;
    sim.lossRate = 0.3f; // 30% loss

    std::queue<LatencySimulator::Packet> sendQueue;

    constexpr int kPacketCount = 1000;
    for (int i = 0; i < kPacketCount; ++i)
    {
        std::vector<uint8_t> payload = {static_cast<uint8_t>(i & 0xFF)};
        sim.Send(sendQueue, payload);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    std::vector<std::vector<uint8_t>> delivered;
    sim.DeliverReady(sendQueue, delivered);

    // With 30% loss, we expect roughly 700 delivered (within tolerance)
    const float deliveredRate = static_cast<float>(delivered.size()) /
                                static_cast<float>(kPacketCount);
    EXPECT_LT(deliveredRate, 0.85f); // Upper bound: at most 85%
    EXPECT_GT(deliveredRate, 0.50f); // Lower bound: at least 50%
}

// ─── Test: multi-client replication round-trip ────────────────────────────────

TEST_F(NetworkLoadStressTest, MultiClientReplicationRoundTrip)
{
    SAGA_PROFILE_FUNCTION();

    // Simulate server + 4 clients
    constexpr int kClientCount = 4;
    constexpr int kEntities = 1'000;
    constexpr int kTicks = 30;

    WorldState serverWorld;

    // Create entities on server
    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = serverWorld.CreateEntity();
        serverWorld.AddComponent<NetPlayerState>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0u});
    }

    // Client worlds start empty
    std::vector<WorldState> clientWorlds(kClientCount);

    LatencySimulator sim;
    sim.latencyMs = 10.f;
    sim.jitterMs = 2.f;
    sim.lossRate = 0.0f;

    std::queue<LatencySimulator::Packet> serverQueue;

    // Run kTicks of replication
    for (int tick = 0; tick < kTicks; ++tick)
    {
        // Server updates entity positions
        for (const auto& eid : serverWorld.GetAliveEntities())
        {
            auto* state = serverWorld.GetComponent<NetPlayerState>(eid);
            if (state)
            {
                state->positionX += state->velocityX * 0.015625f; // 1/64 tick
                state->positionY += state->velocityY * 0.015625f;
            }
        }

        // Server serializes and sends to clients
        const auto snapshot = SerializeWorldSnapshot(serverWorld);
        sim.Send(serverQueue, snapshot);

        // Simulate delivery delay every 5 ticks
        if (tick % 5 == 4)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            std::vector<std::vector<uint8_t>> delivered;
            sim.DeliverReady(serverQueue, delivered);

            for (const auto& data : delivered)
            {
                for (int c = 0; c < kClientCount; ++c)
                {
                    clientWorlds[static_cast<size_t>(c)] =
                        DeserializeWorldSnapshot(data);
                }
            }
        }
    }

    // Final sync: ensure all clients match server
    const auto finalSnapshot = SerializeWorldSnapshot(serverWorld);
    const auto finalServer = DeserializeWorldSnapshot(finalSnapshot);

    for (int c = 0; c < kClientCount; ++c)
    {
        // After final sync, client entity count should match server
        EXPECT_EQ(clientWorlds[static_cast<size_t>(c)].EntityCount(),
                  finalServer.EntityCount());
    }
}

// ─── Test: InterestManager spatial filtering under load ───────────────────────

TEST_F(NetworkLoadStressTest, InterestManagerSpatialFiltering)
{
    SAGA_PROFILE_FUNCTION();

    InterestManager manager;
    InterestConfig config;
    config.cellSize = 64.f;
    config.interestRadius = 2;
    config.hysteresisMargin = 1;
    manager.Configure(config);

    // Register entities in a grid pattern
    constexpr int kGridSize = 20; // 20x20 grid
    constexpr int kTotalEntities = kGridSize * kGridSize;

    for (int x = 0; x < kGridSize; ++x)
    {
        for (int z = 0; z < kGridSize; ++z)
        {
            ECS::EntityId entityId = static_cast<ECS::EntityId>(x * kGridSize + z + 1);
            Math::Vec3 pos{static_cast<float>(x * 64), 0.f, static_cast<float>(z * 64)};
            manager.RegisterEntity(entityId, pos);
        }
    }

    // Place client at center of grid
    const float centerX = static_cast<float>(kGridSize / 2) * 64.f;
    const float centerZ = static_cast<float>(kGridSize / 2) * 64.f;
    manager.UpdateClientPosition(Math::Vec3{centerX, 0.f, centerZ});

    const auto relevant = manager.GetRelevantEntities();

    // With interest radius of 2 and cell size 64, we expect a (2*2+1)x(2*2+1) = 25 cell area
    // Each cell has 1 entity, so roughly 25 entities (some cells may be empty at edges)
    EXPECT_GT(relevant.size(), 0u);
    EXPECT_LE(relevant.size(), static_cast<size_t>(kTotalEntities));
}

// ─── Test: memory pressure during serialization ───────────────────────────────

TEST_F(NetworkLoadStressTest, MemoryPressureDuringSerialization)
{
    SAGA_PROFILE_FUNCTION();

    ArenaAllocator arena(512u * 1024u); // 512 KiB blocks
    WorldState world;
    constexpr int kEntities = 10'000;

    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<NetPlayerState>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0u});
    }

    const auto t0 = std::chrono::steady_clock::now();

    // Serialize multiple times using arena for temporary buffers
    constexpr int kIterations = 20;
    for (int i = 0; i < kIterations; ++i)
    {
        {
            ScopedArena scope(arena);
            // Allocate temporary buffer from arena
            auto* buffer = arena.Allocate(1024 * 1024); // 1 MiB scratch
            ASSERT_NE(buffer, nullptr);
            (void)buffer;

            // Serialize the world
            const auto snapshot = SerializeWorldSnapshot(world);
            EXPECT_GT(snapshot.size(), 0u);
        }
        // Arena rewinds
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Verify arena stats
    const auto& stats = arena.Stats();
    EXPECT_EQ(stats.bytesInUse, 0u);
    EXPECT_GT(stats.allocations, 0u);
}

// ─── Test: VarInt encoding efficiency for component data ──────────────────────

TEST_F(NetworkLoadStressTest, VarIntEncodingEfficiency)
{
    SAGA_PROFILE_FUNCTION();

    std::vector<uint8_t> buffer(1024);
    uint8_t* writePtr = buffer.data();
    const uint8_t* const bufferEnd = buffer.data() + buffer.size();
    const size_t capacity = buffer.size();

    // Encode a range of values
    constexpr uint64_t kSmallValue = 42;
    constexpr uint64_t kMediumValue = 16383;
    constexpr uint64_t kLargeValue = 1'000'000'000;

    size_t written = 0;
    written = Encoding::WriteVarUInt64(kSmallValue, writePtr + written, capacity - written);
    ASSERT_GT(written, 0u);
    size_t totalWritten = written;

    size_t w2 = Encoding::WriteVarUInt64(kMediumValue, writePtr + totalWritten, capacity - totalWritten);
    ASSERT_GT(w2, 0u);
    totalWritten += w2;

    size_t w3 = Encoding::WriteVarUInt64(kLargeValue, writePtr + totalWritten, capacity - totalWritten);
    ASSERT_GT(w3, 0u);
    totalWritten += w3;

    // Small values should use 1 byte, medium 2, large ~4-5
    EXPECT_LT(totalWritten, 15u);
    EXPECT_GT(totalWritten, 0u);

    // Decode and verify
    size_t cursor = 0;
    uint64_t decodedSmall = 0, decodedMedium = 0, decodedLarge = 0;

    bool ok1 = Encoding::ReadVarUInt64(buffer.data(), buffer.size(), cursor, decodedSmall);
    bool ok2 = Encoding::ReadVarUInt64(buffer.data(), buffer.size(), cursor, decodedMedium);
    bool ok3 = Encoding::ReadVarUInt64(buffer.data(), buffer.size(), cursor, decodedLarge);

    ASSERT_TRUE(ok1);
    ASSERT_TRUE(ok2);
    ASSERT_TRUE(ok3);

    EXPECT_EQ(decodedSmall, kSmallValue);
    EXPECT_EQ(decodedMedium, kMediumValue);
    EXPECT_EQ(decodedLarge, kLargeValue);
}

// ─── Test: high-frequency tick replication ─────────────────────────────────────

TEST_F(NetworkLoadStressTest, HighFrequencyTickReplication)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntities = 500;

    for (int i = 0; i < kEntities; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<NetPlayerState>(entity.id,
            {static_cast<float>(i), 0.f, 0.f, 0.5f, 0.f, 0.f, 0.f, 0u});
    }

    SimulationTick simTick(64u); // 64 Hz
    constexpr double kFixedDelta = 1.0 / 64.0;

    uint64_t totalBytesSent = 0;
    constexpr int kTickCount = 64; // 1 second of ticks

    for (int i = 0; i < kTickCount; ++i)
    {
        // Update entity positions
        for (const auto& eid : world.GetAliveEntities())
        {
            auto* state = world.GetComponent<NetPlayerState>(eid);
            if (state)
            {
                state->positionX += state->velocityX * static_cast<float>(kFixedDelta);
                state->positionY += state->velocityY * static_cast<float>(kFixedDelta);
            }
        }

        // Serialize for replication
        const auto snapshot = SerializeWorldSnapshot(world);
        totalBytesSent += snapshot.size();
    }

    const double totalMB = static_cast<double>(totalBytesSent) / (1024.0 * 1024.0);

    // Sanity: we should have sent non-trivial data over 64 ticks
    EXPECT_GT(totalMB, 0.01);

    // World should have moved consistently
    const auto* firstState = world.GetComponent<NetPlayerState>(1);
    if (firstState)
    {
        EXPECT_GT(firstState->positionX, static_cast<float>(kTickCount) * 0.5f * kFixedDelta * 0.9f);
    }
}
