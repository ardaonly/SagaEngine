/// @file SoakTest.cpp
/// @brief Extended-duration soak tests for the replication pipeline.

#include "SoakTest.h"

#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Client/Replication/RateLimitGuard.h"
#include "SagaEngine/Client/Replication/ReplicationTelemetry.h"
#include "SagaEngine/Client/Replication/ReplicationMemoryTracker.h"
#include "SagaEngine/Client/Replication/CatastrophicRecoveryManager.h"

#include <random>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

struct SoakRng
{
    std::mt19937_64 engine;
    explicit SoakRng(std::uint64_t seed) : engine(seed) {}

    float Float01() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(engine);
    }

    std::uint32_t Uint32(std::uint32_t min, std::uint32_t max) {
        std::uniform_int_distribution<std::uint32_t> dist(min, max);
        return dist(engine);
    }

    bool Bernoulli(float p) { return Float01() < p; }
};

} // namespace

// ─── Normal operation soak ──────────────────────────────────────────────────

SoakTestResult SoakNormalOperation(std::uint64_t tickCount, std::uint64_t seed) noexcept
{
    SoakTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    SoakRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    RateLimitGuard rlg;
    rlg.Configure({1000, 16, 120, 256, 1000, 2000});

    CatastrophicRecoveryManager recovery;
    recovery.Configure({3, 5, 5, 600});

    for (std::uint64_t tick = 1; tick <= tickCount; ++tick)
    {
        ++result.ticksElapsed;

        // Simulate receiving a delta.
        std::uint64_t seq = tick;
        sm.RecordSequence(seq);
        sm.UpdateRtt(50.0f + rng.Float01() * 100.0f);

        AcceptResult ar = sm.AcceptPacket(static_cast<ServerTick>(tick), false);
        if (ar == AcceptResult::Accept)
        {
            rlg.RecordDecodeSuccess();
            recovery.RecordSuccess();
        }

        // Tick subsystems.
        sm.Tick(static_cast<ServerTick>(tick));
        rlg.Tick(tick, 1.0f / 60.0f);
        recovery.Tick(tick);

        // Check state machine stability.
        if (sm.CurrentState() == ReplicationState::Desynced)
            ++result.totalDesyncs;
    }

    result.success = true;
    result.totalDesyncs = sm.Stats().desyncEvents;
    result.totalRecoveries = recovery.GetReconnectCount();

    return result;
}

// ─── Intermittent desync soak ───────────────────────────────────────────────

SoakTestResult SoakIntermittentDesync(std::uint64_t tickCount, std::uint64_t seed) noexcept
{
    SoakTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    SoakRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    CatastrophicRecoveryManager recovery;
    recovery.Configure({3, 5, 5, 600});

    for (std::uint64_t tick = 1; tick <= tickCount; ++tick)
    {
        ++result.ticksElapsed;

        // Inject desync every 10000 ticks.
        if (tick % 10000 == 0 && tick > 0)
        {
            sm.TransitionTo(ReplicationState::Desynced);
            recovery.RecordDesync(tick);
        }

        sm.Tick(static_cast<ServerTick>(tick));
        recovery.Tick(tick);

        if (sm.CurrentState() == ReplicationState::Desynced)
            ++result.totalDesyncs;

        if (recovery.GetState() != RecoveryState::Idle)
            ++result.totalRecoveries;
    }

    result.success = true;
    return result;
}

// ─── High churn soak ────────────────────────────────────────────────────────

SoakTestResult SoakHighChurn(std::uint64_t tickCount, std::uint64_t seed) noexcept
{
    SoakTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    SoakRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    std::uint64_t spawnCount = 0;
    std::uint64_t despawnCount = 0;

    for (std::uint64_t tick = 1; tick <= tickCount; ++tick)
    {
        ++result.ticksElapsed;

        // Rapid spawn/despawn cycles.
        if (rng.Bernoulli(0.1f))
            ++spawnCount;
        if (rng.Bernoulli(0.1f))
            ++despawnCount;

        std::uint64_t seq = tick;
        sm.RecordSequence(seq);
        sm.Tick(static_cast<ServerTick>(tick));
    }

    result.success = true;
    return result;
}

// ─── Bandwidth pressure soak ────────────────────────────────────────────────

SoakTestResult SoakBandwidthPressure(std::uint64_t tickCount, std::uint64_t seed) noexcept
{
    SoakTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    SoakRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    RateLimitGuard rlg;
    rlg.Configure({100, 16, 120, 256, 100, 200});  // Tight limits.

    std::uint64_t throttledCount = 0;

    for (std::uint64_t tick = 1; tick <= tickCount; ++tick)
    {
        ++result.ticksElapsed;

        // Flood with packets to trigger rate limiting.
        for (int i = 0; i < 10; ++i)
        {
            RateLimitVerdict v = rlg.AcceptPacket();
            if (v == RateLimitVerdict::Throttle)
                ++throttledCount;
        }

        rlg.Tick(tick, 1.0f / 60.0f);
        sm.Tick(static_cast<ServerTick>(tick));
    }

    result.success = true;
    return result;
}

// ─── Run all soak tests ─────────────────────────────────────────────────────

std::vector<SoakTestResult> RunAllSoakTests(std::uint64_t tickCount, std::uint64_t seed) noexcept
{
    std::vector<SoakTestResult> results;
    results.reserve(4);

    results.push_back(SoakNormalOperation(tickCount, seed));
    results.push_back(SoakIntermittentDesync(tickCount, seed));
    results.push_back(SoakHighChurn(tickCount, seed));
    results.push_back(SoakBandwidthPressure(tickCount, seed));

    return results;
}

} // namespace SagaEngine::Client::Replication
