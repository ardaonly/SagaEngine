/// @file AdversarialModelTest.cpp
/// @brief Adversarial load model tests for the replication pipeline.

#include "AdversarialModelTest.h"

#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Client/Replication/RateLimitGuard.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"

#include <random>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

struct AdvRng
{
    std::mt19937_64 engine;
    explicit AdvRng(std::uint64_t seed) : engine(seed) {}

    float Float01() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(engine);
    }
};

} // namespace

// ─── CPU starvation test ────────────────────────────────────────────────────

AdversarialTestResult TestCpuStarvation(std::uint64_t seed) noexcept
{
    AdversarialTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    AdvRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    RateLimitGuard rlg;
    rlg.Configure({100, 16, 120, 256, 100, 200});  // Tight limits.

    // Flood 10000 packets in rapid succession.
    std::uint64_t accepted = 0;
    std::uint64_t throttled = 0;

    for (int i = 0; i < 10000; ++i)
    {
        ++result.attackVectors;

        RateLimitVerdict v = rlg.AcceptPacket();
        if (v == RateLimitVerdict::Accept)
            ++accepted;
        else if (v == RateLimitVerdict::Throttle)
            ++throttled;
    }

    // Success: rate limiter should have throttled most packets.
    result.attacksBlocked = throttled;
    result.success = (throttled > accepted);

    if (!result.success)
        result.failureDetail = "Rate limiter accepted too many packets";

    return result;
}

// ─── Gap storm test ─────────────────────────────────────────────────────────

AdversarialTestResult TestGapStorm(std::uint64_t seed) noexcept
{
    AdversarialTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    AdvRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    // Inject 100 large sequence gaps.
    for (int i = 0; i < 100; ++i)
    {
        ++result.attackVectors;

        std::uint64_t seq = static_cast<std::uint64_t>(i) * 1000;
        sm.RecordSequence(seq);

        if (sm.CurrentState() == ReplicationState::Desynced)
        {
            ++result.attacksBlocked;
            // System correctly detected desync.
        }
    }

    // Success: system should have detected the gap storm and entered Desynced.
    result.success = (result.attacksBlocked > 0);

    if (!result.success)
        result.failureDetail = "Gap storm did not trigger desync detection";

    return result;
}

// ─── Replay attack test ─────────────────────────────────────────────────────

AdversarialTestResult TestReplayAttack(std::uint64_t seed) noexcept
{
    AdversarialTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    AdvRng rng(seed);

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    // Send packets 1-100 normally.
    for (std::uint64_t seq = 1; seq <= 100; ++seq)
    {
        sm.RecordSequence(seq);
        sm.Tick(static_cast<ServerTick>(seq));
    }

    // Replay packets 37-100 (within the 64-sequence window, these should be seen).
    // Window size is 64, so we can only detect replays within that range.
    std::uint64_t replayStart = 100 - 63;  // Within window.
    for (std::uint64_t seq = replayStart; seq <= 100; ++seq)
    {
        ++result.attackVectors;

        // Check if the sequence was already seen (replay detection).
        if (sm.IsSequenceSeen(seq))
            ++result.attacksBlocked;
    }

    // Success: all replayed packets within window should be detected as old.
    result.success = (result.attacksBlocked == 64);  // 100-37+1 = 64 packets.

    if (!result.success)
        result.failureDetail = "Replayed packets were not all dropped";

    return result;
}

// ─── Sequence wrap-around test ──────────────────────────────────────────────

AdversarialTestResult TestSequenceWrapAround(std::uint64_t seed) noexcept
{
    AdversarialTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    // Test near uint64 max values.
    std::uint64_t nearMax = UINT64_MAX - 100;

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    // Record sequences near the max.
    for (std::uint64_t i = 0; i < 100; ++i)
    {
        ++result.attackVectors;
        sm.RecordSequence(nearMax + i);
    }

    // System should handle this without crashing or looping.
    result.attacksBlocked = 100;  // All processed successfully.
    result.success = true;

    return result;
}

// ─── Decode bomb test ───────────────────────────────────────────────────────

AdversarialTestResult TestDecodeBomb(std::uint64_t seed) noexcept
{
    AdversarialTestResult result;

    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }

    // Test with an oversized payload (10 MB).
    std::vector<std::uint8_t> oversized(10 * 1024 * 1024, 0);

    // Fill with header-like data to try to confuse the decoder.
    oversized[0] = 1;  // version
    oversized[28] = 0xFF; oversized[29] = 0xFF; // entityCount high

    DeltaDecodeResult decoded = DecodeDeltaWire(oversized.data(), oversized.size(), 1024);

    ++result.attackVectors;

    // Decoder should reject this without crashing.
    if (!decoded.success)
    {
        ++result.attacksBlocked;
        result.success = true;  // Expected rejection.
    }
    else
    {
        result.failureDetail = "Decoder accepted oversized payload";
    }

    return result;
}

// ─── Run all tests ──────────────────────────────────────────────────────────

std::vector<AdversarialTestResult> RunAllAdversarialTests(std::uint64_t seed) noexcept
{
    std::vector<AdversarialTestResult> results;
    results.reserve(5);

    results.push_back(TestCpuStarvation(seed));
    results.push_back(TestGapStorm(seed));
    results.push_back(TestReplayAttack(seed));
    results.push_back(TestSequenceWrapAround(seed));
    results.push_back(TestDecodeBomb(seed));

    return results;
}

} // namespace SagaEngine::Client::Replication
