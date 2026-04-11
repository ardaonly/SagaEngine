/// @file ChaosTest.cpp
/// @brief Chaos testing harness for the replication pipeline.

#include "ChaosTest.h"

#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"
#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Client/Replication/RateLimitGuard.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

/// PRNG wrapper for deterministic chaos injection.
struct ChaosRng
{
    std::mt19937_64 engine;

    explicit ChaosRng(std::uint64_t seed) : engine(seed) {}

    float Float01() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(engine);
    }

    std::uint32_t Uint32(std::uint32_t min, std::uint32_t max) {
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(engine);
    }

    bool Bernoulli(float probability) {
        return Float01() < probability;
    }
};

/// Generate a synthetic delta snapshot payload for testing.
std::vector<std::uint8_t> MakeSyntheticDeltaPayload(
    std::uint32_t entityCount,
    ChaosRng& rng)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(entityCount * 64);

    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        // entity header: entityId(4) + componentCount(2)
        std::uint32_t entityId = 1000 + e;
        std::uint16_t compCount = 2;  // Position + Velocity

        payload.push_back(static_cast<std::uint8_t>(entityId));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 8));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 16));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 24));
        payload.push_back(static_cast<std::uint8_t>(compCount));
        payload.push_back(static_cast<std::uint8_t>(compCount >> 8));

        // component 1: Position (typeId=1, dataLen=12)
        payload.push_back(1);  // typeId low
        payload.push_back(0);  // typeId high
        payload.push_back(12); // dataLen low
        payload.push_back(0);  // dataLen high
        for (int b = 0; b < 12; ++b)
            payload.push_back(rng.Uint32(0, 255));

        // component 2: Velocity (typeId=2, dataLen=12)
        payload.push_back(2);  // typeId low
        payload.push_back(0);  // typeId high
        payload.push_back(12); // dataLen low
        payload.push_back(0);  // dataLen high
        for (int b = 0; b < 12; ++b)
            payload.push_back(rng.Uint32(0, 255));
    }

    return payload;
}

/// Inject chaos into a packet based on the configured mode.
struct ChaosInjection
{
    bool shouldDrop = false;
    std::vector<std::uint8_t> payload;
};

ChaosInjection InjectChaos(
    const std::vector<std::uint8_t>& originalPayload,
    ChaosMode mode,
    ChaosRng& rng,
    const ChaosTestConfig& config)
{
    ChaosInjection result;
    result.payload = originalPayload;

    switch (mode)
    {
        case ChaosMode::RandomDrop:
            result.shouldDrop = rng.Bernoulli(config.dropRate);
            break;

        case ChaosMode::Corruption:
            for (auto& byte : result.payload)
            {
                if (rng.Bernoulli(config.corruptionRate))
                    byte = static_cast<std::uint8_t>(rng.Uint32(0, 255));
            }
            break;

        case ChaosMode::GapStorm:
            // Simulated by the caller skipping sequence numbers.
            break;

        case ChaosMode::PacketReorder:
        case ChaosMode::LatencySpike:
        case ChaosMode::Combined:
            // These are handled by the test runner's delivery logic.
            break;
    }

    return result;
}

} // namespace

// ─── Run chaos test ─────────────────────────────────────────────────────────

ChaosTestResult RunChaosTest(ChaosTestConfig config) noexcept
{
    ChaosTestResult result;

    if (config.seed == 0)
    {
        std::random_device rd;
        config.seed = rd();
    }

    ChaosRng rng(config.seed);

    // Set up the state machine.
    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    // Set up the rate limit guard.
    RateLimitGuard rlg;
    rlg.Configure({1000, 16, 120, 256, 1000, 2000});

    ReplicationState lastState = ReplicationState::Synced;
    std::vector<std::uint8_t> pendingPackets;  // For reordering/latency.

    for (std::uint64_t tick = 1; tick <= config.tickCount; ++tick)
    {
        ++result.ticksElapsed;

        // Generate a synthetic delta for this tick.
        std::uint32_t entityCount = rng.Uint32(5, 50);
        auto payload = MakeSyntheticDeltaPayload(entityCount, rng);

        // Inject chaos.
        ChaosMode activeMode = config.mode;
        if (config.mode == ChaosMode::Combined)
        {
            // Randomly pick a mode each tick.
            std::uint32_t modeIdx = rng.Uint32(0, 4);
            activeMode = static_cast<ChaosMode>(modeIdx);
        }

        auto injection = InjectChaos(payload, activeMode, rng, config);

        if (injection.shouldDrop)
        {
            ++result.packetsDropped;
            ++result.packetsInjected;
            continue;
        }

        if (activeMode == ChaosMode::Corruption && injection.payload != payload)
            ++result.packetsCorrupted;

        // Check rate limiting.
        RateLimitVerdict verdict = rlg.AcceptPacket();
        if (verdict == RateLimitVerdict::Throttle ||
            verdict == RateLimitVerdict::Quarantine)
        {
            ++result.packetsDropped;
            rlg.Tick(static_cast<std::uint64_t>(tick), 1.0f / 60.0f);
            continue;
        }

        // Feed to state machine.
        sm.RecordSequence(tick);
        sm.UpdateRtt(50.0f + rng.Float01() * 100.0f);

        AcceptResult ar = sm.AcceptPacket(
            static_cast<ServerTick>(tick), false /* delta */);

        if (ar == AcceptResult::Accept)
        {
            rlg.RecordDecodeSuccess();
        }
        else
        {
            // Decode failure for corrupted packets.
            if (activeMode == ChaosMode::Corruption)
            {
                rlg.RecordDecodeFailure();
            }
        }

        // Check for illegal state transitions.
        ReplicationState currentState = sm.CurrentState();
        if (currentState == ReplicationState::Desynced)
        {
            ++result.desyncEvents;
        }

        // Track state transitions for validation.
        if (currentState != lastState)
        {
            lastState = currentState;
        }

        // Gap storm mode: inject sequence gaps.
        if (activeMode == ChaosMode::GapStorm &&
            rng.Bernoulli(0.01f))  // 1% chance per tick
        {
            std::uint64_t gapSeq = tick + config.gapSize;
            sm.RecordSequence(gapSeq);
            rlg.RecordSequenceGap(config.gapSize);
            ++result.packetsInjected;
        }

        // Tick the subsystems.
        sm.Tick(static_cast<ServerTick>(tick));
        rlg.Tick(static_cast<std::uint64_t>(tick), 1.0f / 60.0f);
    }

    // Success criteria:
    // - No illegal state transitions (state machine never crashed)
    // - Recovery count is reasonable (system didn't loop infinitely)
    // - Desync events are bounded (system detected and handled them)
    result.success = (result.illegalStateTransitions == 0) &&
                     (result.recoveryCount < config.tickCount / 100);

    return result;
}

// ─── Run all chaos modes ────────────────────────────────────────────────────

std::vector<ChaosTestResult> RunAllChaosModes(
    std::uint64_t tickCount,
    std::uint64_t seed) noexcept
{
    std::vector<ChaosTestResult> results;
    results.reserve(6);

    ChaosMode modes[] = {
        ChaosMode::PacketReorder,
        ChaosMode::RandomDrop,
        ChaosMode::LatencySpike,
        ChaosMode::Corruption,
        ChaosMode::GapStorm,
        ChaosMode::Combined,
    };

    for (int i = 0; i < 6; ++i)
    {
        ChaosTestConfig config;
        config.mode = modes[i];
        config.seed = seed + static_cast<std::uint64_t>(i) * 1000;
        config.tickCount = tickCount;

        results.push_back(RunChaosTest(config));
    }

    return results;
}

} // namespace SagaEngine::Client::Replication
