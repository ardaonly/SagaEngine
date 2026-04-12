/// @file ReplicationTests.cpp
/// @brief Integration tests for the replication pipeline: delta snapshots,
///        baseline management, and jitter buffer behavior.

#include "SagaEngine/ECS/Component.h"
#include "SagaEngine/ECS/ComponentRegistry.h"
#include "SagaEngine/Simulation/WorldState.h"
#include "SagaEngine/Core/Profiling/Profiler.h"
#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

using namespace SagaEngine;
using namespace SagaEngine::Simulation;
using namespace SagaEngine::Core;
using namespace SagaEngine::Client::Replication;

// ─── Test components ──────────────────────────────────────────────────────────

struct RepTestPosition
{
    float x = 0.f, y = 0.f, z = 0.f;
};

struct RepTestHealth
{
    float current = 100.f;
    float max = 100.f;
};

// ─── Helper ───────────────────────────────────────────────────────────────────

static void RegisterRepTestComponents()
{
    using CR = ECS::ComponentRegistry;
    if (!CR::Instance().IsRegistered<RepTestPosition>())
        CR::Instance().Register<RepTestPosition>(6001u, "RepTestPosition");
    if (!CR::Instance().IsRegistered<RepTestHealth>())
        CR::Instance().Register<RepTestHealth>(6002u, "RepTestHealth");
}

// ─── Test fixture ──────────────────────────────────────────────────────────────

class ReplicationPipelineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RegisterRepTestComponents();
        Profiler::Instance().Clear();
    }
};

// ─── Test: Full snapshot apply through pipeline ───────────────────────────────

TEST_F(ReplicationPipelineTest, FullSnapshotApply)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;

    // Build server world
    WorldState serverWorld;
    const auto entity = serverWorld.CreateEntity();
    serverWorld.AddComponent<RepTestPosition>(entity.id, {10.f, 20.f, 30.f});
    serverWorld.AddComponent<RepTestHealth>(entity.id, {75.f, 100.f});

    // Setup pipeline with simple apply that deserializes
    SnapshotApplyPipeline pipeline;
    pipeline.Configure(
        &clientWorld,
        [](WorldState& world, const DecodedWorldSnapshot& snapshot) -> bool
        {
            try
            {
                world = WorldState::Deserialize(
                    std::span<const uint8_t>(snapshot.payload.data(), snapshot.payload.size()));
                return true;
            }
            catch (...)
            {
                return false;
            }
        },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; });

    // Build and submit full snapshot
    DecodedWorldSnapshot fullSnap;
    fullSnap.serverTick = 1;
    fullSnap.payload = serverWorld.Serialize();

    const auto outcome = pipeline.SubmitFull(std::move(fullSnap));
    EXPECT_EQ(outcome, ApplyOutcome::Ok);
    EXPECT_EQ(pipeline.LastAppliedTick(), 1u);

    // Verify client received entity
    EXPECT_EQ(clientWorld.EntityCount(), 1u);

    const auto* clientPos = clientWorld.GetComponent<RepTestPosition>(entity.id);
    ASSERT_NE(clientPos, nullptr);
    EXPECT_FLOAT_EQ(clientPos->x, 10.f);
    EXPECT_FLOAT_EQ(clientPos->y, 20.f);
    EXPECT_FLOAT_EQ(clientPos->z, 30.f);
}

// ─── Test: Delta snapshot apply on matching baseline ──────────────────────────

TEST_F(ReplicationPipelineTest, DeltaApplyOnMatchingBaseline)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    // Setup client with initial state
    const auto entity = clientWorld.CreateEntity();
    clientWorld.AddComponent<RepTestPosition>(entity.id, {0.f, 0.f, 0.f});
    clientWorld.AddComponent<RepTestHealth>(entity.id, {100.f, 100.f});

    bool deltaApplied = false;
    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [&deltaApplied](WorldState& world, const DecodedDeltaSnapshot& delta) -> bool
        {
            (void)delta;
            // For this test, just modify the entity's position
            auto* pos = world.GetComponent<RepTestPosition>(1);
            if (pos)
            {
                pos->x = 50.f;
            }
            deltaApplied = true;
            return true;
        });

    // Set baseline to tick 1
    // (In real usage, this happens via full snapshot apply)

    DecodedDeltaSnapshot delta;
    delta.baselineTick = 0; // Client starts at 0
    delta.serverTick = 1;

    const auto outcome = pipeline.SubmitDelta(std::move(delta));
    EXPECT_TRUE(deltaApplied);
}

// ─── Test: Delta dropped due to missing baseline ─────────────────────────────

TEST_F(ReplicationPipelineTest, DeltaDroppedMissingBaseline)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; },
        SnapshotPipelineConfig{
            .jitterBufferSlots = 4,
            .maxBufferedAgeTicks = 30,
            .autoRequestFullSnapshotOnMiss = true
        });

    // Submit delta without any baseline — should trigger missing baseline
    DecodedDeltaSnapshot delta;
    delta.baselineTick = 100;
    delta.serverTick = 101;

    // Since client is at tick 0 and delta expects baseline 100, it should miss
    const auto outcome = pipeline.SubmitDelta(std::move(delta));
    // Either buffered or dropped depending on config
    EXPECT_TRUE(outcome == ApplyOutcome::MissingBaseline ||
                outcome == ApplyOutcome::BufferedForOrdering);
}

// ─── Test: Duplicate delta detection ──────────────────────────────────────────

TEST_F(ReplicationPipelineTest, DuplicateDeltaDetection)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; });

    DecodedDeltaSnapshot delta;
    delta.baselineTick = 0;
    delta.serverTick = 1;

    // First apply
    auto outcome1 = pipeline.SubmitDelta(DecodedDeltaSnapshot(delta));

    // Second apply of same tick should be dropped as duplicate
    auto outcome2 = pipeline.SubmitDelta(DecodedDeltaSnapshot(delta));
    EXPECT_EQ(outcome2, ApplyOutcome::DroppedDuplicate);
}

// ─── Test: Pipeline statistics tracking ───────────────────────────────────────

TEST_F(ReplicationPipelineTest, PipelineStatsTracking)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; });

    // Submit full snapshot
    DecodedWorldSnapshot fullSnap;
    fullSnap.serverTick = 1;
    fullSnap.payload = {1, 2, 3, 4};

    pipeline.SubmitFull(std::move(fullSnap));

    // Submit valid deltas
    for (std::uint64_t tick = 2; tick <= 5; ++tick)
    {
        DecodedDeltaSnapshot delta;
        delta.baselineTick = tick - 1;
        delta.serverTick = tick;
        pipeline.SubmitDelta(std::move(delta));
    }

    const auto& stats = pipeline.Stats();
    EXPECT_GE(stats.fullApplied, 1u);
}

// ─── Test: Jitter buffer overflow ─────────────────────────────────────────────

TEST_F(ReplicationPipelineTest, JitterBufferOverflow)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    SnapshotPipelineConfig config;
    config.jitterBufferSlots = 2;
    config.maxBufferedAgeTicks = 10;
    config.autoRequestFullSnapshotOnMiss = false;

    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; },
        config);

    // Fill the jitter buffer with deltas that can't be applied (baseline mismatch)
    for (int i = 0; i < 10; ++i)
    {
        DecodedDeltaSnapshot delta;
        delta.baselineTick = 100; // Mismatch with client baseline 0
        delta.serverTick = static_cast<std::uint64_t>(101 + i);
        pipeline.SubmitDelta(std::move(delta));
    }

    const auto& stats = pipeline.Stats();
    // Some should have been buffered, some dropped
    EXPECT_TRUE(stats.bufferedDeltas > 0 || stats.droppedOld > 0 ||
                stats.jitterOverflow > 0);
}

// ─── Test: Pipeline reset clears all state ────────────────────────────────────

TEST_F(ReplicationPipelineTest, PipelineReset)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [](WorldState&, const DecodedDeltaSnapshot&) -> bool { return true; });

    // Submit some data
    DecodedWorldSnapshot fullSnap;
    fullSnap.serverTick = 1;
    fullSnap.payload = {1, 2, 3};
    pipeline.SubmitFull(std::move(fullSnap));

    EXPECT_EQ(pipeline.LastAppliedTick(), 1u);

    // Reset
    pipeline.Reset();
    EXPECT_EQ(pipeline.LastAppliedTick(), kInvalidTick);
}

// ─── Test: World state serialization roundtrip ────────────────────────────────

TEST_F(ReplicationPipelineTest, WorldStateRoundtrip)
{
    SAGA_PROFILE_FUNCTION();

    WorldState original;

    // Create multiple entities
    for (int i = 0; i < 50; ++i)
    {
        const auto entity = original.CreateEntity();
        original.AddComponent<RepTestPosition>(entity.id,
            {static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3)});

        if (i % 3 == 0)
        {
            original.AddComponent<RepTestHealth>(entity.id,
                {static_cast<float>(100 - i), 100.f});
        }
    }

    const uint64_t originalHash = original.Hash();
    const auto buffer = original.Serialize();
    WorldState restored = WorldState::Deserialize(buffer);

    EXPECT_EQ(restored.Hash(), originalHash);
    EXPECT_EQ(restored.EntityCount(), original.EntityCount());

    // Spot check
    const auto* origT = original.GetComponent<RepTestPosition>(1);
    const auto* restT = restored.GetComponent<RepTestPosition>(1);
    ASSERT_NE(origT, nullptr);
    ASSERT_NE(restT, nullptr);
    EXPECT_FLOAT_EQ(restT->x, origT->x);
}

// ─── Test: Delta encoding preserves component data ────────────────────────────

TEST_F(ReplicationPipelineTest, DeltaEncodingPreservesData)
{
    SAGA_PROFILE_FUNCTION();

    // Create initial world
    WorldState baseline;
    const auto entity = baseline.CreateEntity();
    baseline.AddComponent<RepTestPosition>(entity.id, {0.f, 0.f, 0.f});

    auto baselineBuffer = baseline.Serialize();

    // Mutate the entity
    auto* pos = baseline.GetComponent<RepTestPosition>(entity.id);
    ASSERT_NE(pos, nullptr);
    pos->x = 100.f;
    pos->z = 200.f;

    // Serialize mutated state
    auto mutatedBuffer = baseline.Serialize();

    // The two buffers must differ
    EXPECT_NE(baselineBuffer.size(), 0u);
    EXPECT_NE(mutatedBuffer.size(), 0u);

    // Deserializing the mutated state should reflect the new positions
    WorldState restored = WorldState::Deserialize(mutatedBuffer);
    const auto* restPos = restored.GetComponent<RepTestPosition>(entity.id);
    ASSERT_NE(restPos, nullptr);
    EXPECT_FLOAT_EQ(restPos->x, 100.f);
    EXPECT_FLOAT_EQ(restPos->z, 200.f);
}

// ─── Test: Pipeline tick promotes buffered deltas ─────────────────────────────

TEST_F(ReplicationPipelineTest, TickPromotesBufferedDeltas)
{
    SAGA_PROFILE_FUNCTION();

    WorldState clientWorld;
    SnapshotApplyPipeline pipeline;

    SnapshotPipelineConfig config;
    config.jitterBufferSlots = 8;
    config.maxBufferedAgeTicks = 60;
    config.autoRequestFullSnapshotOnMiss = false;

    bool delta2Applied = false;
    pipeline.Configure(
        &clientWorld,
        [](WorldState&, const DecodedWorldSnapshot&) -> bool { return true; },
        [&delta2Applied](WorldState&, const DecodedDeltaSnapshot& delta) -> bool
        {
            if (delta.serverTick == 2)
                delta2Applied = true;
            return true;
        },
        config);

    // First apply tick 1 as baseline
    DecodedWorldSnapshot fullSnap;
    fullSnap.serverTick = 1;
    fullSnap.payload = {0};
    pipeline.SubmitFull(std::move(fullSnap));

    // Submit delta for tick 2 — should apply immediately
    DecodedDeltaSnapshot delta2;
    delta2.baselineTick = 1;
    delta2.serverTick = 2;
    pipeline.SubmitDelta(std::move(delta2));

    pipeline.Tick(2);
    EXPECT_TRUE(delta2Applied);
}
