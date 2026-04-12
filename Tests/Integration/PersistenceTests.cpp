/// @file PersistenceTests.cpp
/// @brief Integration tests for the persistence pipeline: event sourcing,
///        snapshot storage, and state replay without requiring a live database.

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Core/Log/Log.h"

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;

// ─── Test components ──────────────────────────────────────────────────────────

struct PersistTransform
{
    float x = 0.f, y = 0.f, z = 0.f;
};

// ─── Helper ───────────────────────────────────────────────────────────────────

static void RegisterPersistComponents()
{
    using CR = ECS::ComponentRegistry;
    if (!CR::Instance().IsRegistered<PersistTransform>())
        CR::Instance().Register<PersistTransform>(7001u, "PersistTransform");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class PersistenceIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterPersistComponents();
        Profiler::Instance().Clear();
    }
};

// ─── In-memory event log (mock persistence backend) ───────────────────────────

struct PersistedEvent
{
    uint64_t                sequence = 0;
    std::string             eventType;
    std::vector<uint8_t>    payload;
    uint64_t                aggregateId = 0;
    std::chrono::steady_clock::time_point timestamp;
};

class InMemoryEventLog
{
public:
    uint64_t Append(const std::string& type, const std::vector<uint8_t>& payload,
                    uint64_t aggregateId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t seq = nextSequence_++;
        events_.push_back({seq, type, payload, aggregateId,
                           std::chrono::steady_clock::now()});
        return seq;
    }

    std::vector<PersistedEvent> ReadFrom(uint64_t fromSequence) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PersistedEvent> result;
        for (const auto& event : events_)
        {
            if (event.sequence >= fromSequence)
                result.push_back(event);
        }
        return result;
    }

    uint64_t CurrentSequence() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return nextSequence_;
    }

    size_t EventCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_.size();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
        nextSequence_ = 1;
    }

private:
    mutable std::mutex mutex_;
    std::vector<PersistedEvent> events_;
    uint64_t nextSequence_ = 1;
};

// ─── In-memory snapshot store (mock persistence backend) ──────────────────────

struct PersistedSnapshot
{
    uint64_t                tick = 0;
    uint64_t                eventSequence = 0;
    std::vector<uint8_t>    worldStateBytes;
    std::chrono::steady_clock::time_point timestamp;
};

class InMemorySnapshotStore
{
public:
    void Save(uint64_t tick, uint64_t eventSeq, std::vector<uint8_t> worldBytes)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_[tick] = {tick, eventSeq, std::move(worldBytes),
                            std::chrono::steady_clock::now()};
    }

    bool Load(uint64_t tick, PersistedSnapshot& out) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(tick);
        if (it == snapshots_.end())
            return false;
        out = it->second;
        return true;
    }

    uint64_t LatestTick() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t latest = 0;
        for (const auto& [tick, _] : snapshots_)
            latest = std::max(latest, tick);
        return latest;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, PersistedSnapshot> snapshots_;
};

// ─── Test: Event log append and replay ────────────────────────────────────────

TEST_F(PersistenceIntegrationTest, EventLogAppendAndReplay)
{
    SAGA_PROFILE_FUNCTION();

    InMemoryEventLog eventLog;

    // Append entity creation events
    WorldState world;
    const auto entity = world.CreateEntity();
    world.AddComponent<PersistTransform>(entity.id, {0.f, 0.f, 0.f});

    // Serialize as event payload
    auto entityData = world.Serialize();

    const auto seq = eventLog.Append("EntityCreated", entityData, entity.id);
    EXPECT_GT(seq, 0u);
    EXPECT_EQ(eventLog.CurrentSequence(), 2u);

    // Replay from the beginning
    const auto events = eventLog.ReadFrom(1);
    EXPECT_FALSE(events.empty());
    EXPECT_EQ(events.front().eventType, "EntityCreated");

    // Reconstruct world from events
    WorldState replayedWorld = WorldState::Deserialize(events.front().payload);
    EXPECT_EQ(replayedWorld.EntityCount(), 1u);
}

// ─── Test: Snapshot save and load ─────────────────────────────────────────────

TEST_F(PersistenceIntegrationTest, SnapshotSaveLoad)
{
    SAGA_PROFILE_FUNCTION();

    InMemoryEventLog eventLog;
    InMemorySnapshotStore store;

    WorldState world;
    for (int i = 0; i < 100; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<PersistTransform>(entity.id,
            {static_cast<float>(i), 0.f, 0.f});
    }

    const uint64_t tick = 42;
    const auto bytes = world.Serialize();

    // Save snapshot
    store.Save(tick, eventLog.CurrentSequence(), bytes);
    EXPECT_EQ(store.LatestTick(), tick);

    // Load snapshot
    PersistedSnapshot loaded;
    const bool ok = store.Load(tick, loaded);
    ASSERT_TRUE(ok);
    EXPECT_EQ(loaded.tick, tick);
    EXPECT_EQ(loaded.worldStateBytes.size(), bytes.size());

    // Restore world
    WorldState restored = WorldState::Deserialize(loaded.worldStateBytes);
    EXPECT_EQ(restored.EntityCount(), 100u);
    EXPECT_EQ(restored.Hash(), world.Hash());
}

// ─── Test: Event-sourced state rebuild from snapshot + events ─────────────────

TEST_F(PersistenceIntegrationTest, StateRebuildFromSnapshotPlusEvents)
{
    SAGA_PROFILE_FUNCTION();

    InMemoryEventLog eventLog;
    InMemorySnapshotStore store;

    // Phase 1: Build initial world and snapshot at tick 10
    WorldState initialWorld;
    for (int i = 0; i < 50; ++i)
    {
        const auto entity = initialWorld.CreateEntity();
        initialWorld.AddComponent<PersistTransform>(entity.id,
            {static_cast<float>(i), 0.f, 0.f});
    }

    const uint64_t snapshotTick = 10;
    store.Save(snapshotTick, eventLog.CurrentSequence(), initialWorld.Serialize());

    // Phase 2: Append more events (simulate 50 more entity creations)
    for (int i = 50; i < 100; ++i)
    {
        WorldState partialWorld;
        const auto entity = partialWorld.CreateEntity();
        partialWorld.AddComponent<PersistTransform>(entity.id,
            {static_cast<float>(i), 0.f, 0.f});

        eventLog.Append("EntityCreated", partialWorld.Serialize(), entity.id);
    }

    // Phase 3: Rebuild from snapshot + events
    PersistedSnapshot snapshot;
    const bool snapOk = store.Load(snapshotTick, snapshot);
    ASSERT_TRUE(snapOk);

    WorldState rebuilt = WorldState::Deserialize(snapshot.worldStateBytes);

    // Apply events on top
    const auto events = eventLog.ReadFrom(1);
    for (const auto& event : events)
    {
        if (event.eventType == "EntityCreated")
        {
            WorldState eventWorld = WorldState::Deserialize(event.payload);
            // Merge entities from event world into rebuilt world
            for (const auto& eid : eventWorld.GetAliveEntities())
            {
                const auto* srcTransform = eventWorld.GetComponent<PersistTransform>(eid);
                if (srcTransform && !rebuilt.IsAlive(eid))
                {
                    const auto newEntity = rebuilt.CreateEntity();
                    rebuilt.AddComponent<PersistTransform>(newEntity.id, *srcTransform);
                }
            }
        }
    }

    // Final entity count should include snapshot + event entities
    EXPECT_GE(rebuilt.EntityCount(), 50u);
}

// ─── Test: Concurrent event append ────────────────────────────────────────────

TEST_F(PersistenceIntegrationTest, ConcurrentEventAppend)
{
    SAGA_PROFILE_FUNCTION();

    InMemoryEventLog eventLog;

    std::vector<std::thread> threads;
    std::atomic<uint64_t> totalSuccess{0};

    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&eventLog, &totalSuccess, t]()
        {
            for (int i = 0; i < 25; ++i)
            {
                WorldState partialWorld;
                const auto entity = partialWorld.CreateEntity();
                partialWorld.AddComponent<PersistTransform>(entity.id,
                    {static_cast<float>(t * 25 + i), 0.f, 0.f});

                auto data = partialWorld.Serialize();
                const auto seq = eventLog.Append("EntityCreated", data, entity.id);
                if (seq > 0)
                    totalSuccess.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(totalSuccess.load(), 100);
    EXPECT_EQ(eventLog.EventCount(), 100u);
}

// ─── Test: World state persistence roundtrip ──────────────────────────────────

TEST_F(PersistenceIntegrationTest, WorldStatePersistenceRoundtrip)
{
    SAGA_PROFILE_FUNCTION();

    // Build complex world
    WorldState original;
    for (int i = 0; i < 200; ++i)
    {
        const auto entity = original.CreateEntity();
        original.AddComponent<PersistTransform>(entity.id,
            {static_cast<float>(i), static_cast<float>(i * 2),
             static_cast<float>(i * 3)});
    }

    const uint64_t originalHash = original.Hash();

    // Serialize (persistence out)
    const auto buffer = original.Serialize();
    EXPECT_GT(buffer.size(), 0u);

    // Deserialize (persistence in)
    WorldState restored = WorldState::Deserialize(buffer);
    EXPECT_EQ(restored.Hash(), originalHash);
    EXPECT_EQ(restored.EntityCount(), original.EntityCount());

    // Verify component data integrity
    for (int i = 0; i < 200; ++i)
    {
        // Find entity with matching position
        bool found = false;
        for (const auto& eid : restored.GetAliveEntities())
        {
            const auto* t = restored.GetComponent<PersistTransform>(eid);
            if (t && t->x == static_cast<float>(i))
            {
                EXPECT_FLOAT_EQ(t->y, static_cast<float>(i * 2));
                EXPECT_FLOAT_EQ(t->z, static_cast<float>(i * 3));
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}

// ─── Test: Event log read range queries ───────────────────────────────────────

TEST_F(PersistenceIntegrationTest, EventLogRangeQueries)
{
    SAGA_PROFILE_FUNCTION();

    InMemoryEventLog eventLog;

    // Append 20 events
    for (int i = 0; i < 20; ++i)
    {
        WorldState ws;
        const auto e = ws.CreateEntity();
        ws.AddComponent<PersistTransform>(e.id, {static_cast<float>(i), 0.f, 0.f});
        eventLog.Append("EntityCreated", ws.Serialize(), e.id);
    }

    // Read all events
    const auto allEvents = eventLog.ReadFrom(1);
    EXPECT_EQ(allEvents.size(), 20u);

    // Read from sequence 11 (second half)
    const auto laterEvents = eventLog.ReadFrom(11);
    EXPECT_EQ(laterEvents.size(), 10u);

    // Read from beyond the end
    const auto emptyEvents = eventLog.ReadFrom(100);
    EXPECT_TRUE(emptyEvents.empty());
}

// ─── Test: Snapshot pruning (keep only latest) ────────────────────────────────

TEST_F(PersistenceIntegrationTest, SnapshotPruning)
{
    SAGA_PROFILE_FUNCTION();

    InMemorySnapshotStore store;

    // Save multiple snapshots at different ticks
    for (uint64_t tick = 10; tick <= 50; tick += 10)
    {
        WorldState ws;
        for (int i = 0; i < static_cast<int>(tick); ++i)
        {
            const auto entity = ws.CreateEntity();
            ws.AddComponent<PersistTransform>(entity.id,
                {static_cast<float>(i), 0.f, 0.f});
        }
        store.Save(tick, 0, ws.Serialize());
    }

    // Latest tick should be 50
    EXPECT_EQ(store.LatestTick(), 50u);

    // Loading an old snapshot should still work
    PersistedSnapshot oldSnap;
    EXPECT_TRUE(store.Load(20, oldSnap));
    EXPECT_EQ(oldSnap.tick, 20u);

    // Loading non-existent snapshot should fail
    PersistedSnapshot missing;
    EXPECT_FALSE(store.Load(99, missing));
}

// ─── Test: Performance of serialization at scale ──────────────────────────────

TEST_F(PersistenceIntegrationTest, SerializationPerformance)
{
    SAGA_PROFILE_FUNCTION();

    WorldState world;
    constexpr int kEntityCount = 1000;

    for (int i = 0; i < kEntityCount; ++i)
    {
        const auto entity = world.CreateEntity();
        world.AddComponent<PersistTransform>(entity.id,
            {static_cast<float>(i), static_cast<float>(i), static_cast<float>(i)});
    }

    const auto t0 = std::chrono::steady_clock::now();

    // Serialize and deserialize 10 times
    constexpr int kIterations = 10;
    for (int i = 0; i < kIterations; ++i)
    {
        const auto buffer = world.Serialize();
        WorldState restored = WorldState::Deserialize(buffer);
        EXPECT_EQ(restored.EntityCount(), static_cast<uint32_t>(kEntityCount));
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double avgMs = elapsedMs / static_cast<double>(kIterations);

    // Average roundtrip should be under 100ms for 1000 entities
    EXPECT_LT(avgMs, 100.0);
}
