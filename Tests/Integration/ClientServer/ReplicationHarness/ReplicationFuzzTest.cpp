/// @file ReplicationFuzzTest.cpp
/// @brief Fuzz and chaos test infrastructure for the replication pipeline.

#include "ReplicationFuzzTest.h"

#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"
#include "SagaEngine/Client/Replication/WorldSnapshotWire.h"
#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

// ─── PRNG helper ───────────────────────────────────────────────────────────

struct FuzzRng
{
    std::mt19937_64 rng;

    explicit FuzzRng(std::uint64_t seed) : rng(seed) {}

    std::uint8_t Byte() { return static_cast<std::uint8_t>(rng() & 0xFF); }

    std::uint16_t Uint16() { return static_cast<std::uint16_t>(rng() & 0xFFFF); }

    std::uint32_t Uint32() { return static_cast<std::uint32_t>(rng()); }

    /// Generate a random byte vector.
    std::vector<std::uint8_t> Bytes(std::size_t min, std::size_t max)
    {
        std::size_t len = min + (rng() % (max - min + 1));
        std::vector<std::uint8_t> out(len);
        for (auto& b : out)
            b = Byte();
        return out;
    }

    /// Pick a random element from a vector.
    template <typename T>
    const T& Pick(const std::vector<T>& vec)
    {
        return vec[rng() % vec.size()];
    }
};

/// Convert a byte vector to a hex dump string for failure reporting.
std::string HexDump(const std::vector<std::uint8_t>& data, std::size_t maxBytes = 128)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(std::min(data.size(), maxBytes) * 3);
    for (std::size_t i = 0; i < std::min(data.size(), maxBytes); ++i)
    {
        out += hex[data[i] >> 4];
        out += hex[data[i] & 0xF];
        out += ' ';
    }
    return out;
}

} // namespace

// ─── Fuzz delta wire decoder ───────────────────────────────────────────────

FuzzTestResult FuzzDeltaWireDecoder(std::uint64_t iterations, std::uint64_t seed)
{
    FuzzTestResult result;
    result.seed = seed;

    if (seed == 0)
    {
        std::random_device rd;
        result.seed = rd();
    }

    FuzzRng rng(result.seed);

    for (std::uint64_t i = 0; i < iterations; ++i)
    {
        ++result.iterations;

        // Generate a random payload (biased toward small sizes for speed).
        std::size_t payloadSize = rng.Bytes(0, 512).size();
        std::vector<std::uint8_t> payload = rng.Bytes(0, payloadSize);

        // Try to decode.  The decoder should never crash.
        DeltaDecodeResult decodeResult = DecodeDeltaWire(
            payload.data(), payload.size(), 1024);

        // If decode succeeds, validate invariants.
        if (decodeResult.success)
        {
            // Entity count must match decoded entities.
            if (decodeResult.decoded.entities.size() != decodeResult.decoded.entityCount)
            {
                ++result.failures;
                result.lastFailureInput = HexDump(payload);
                result.errorMessage = "entity count mismatch";
                result.success = false;
                break;
            }

            // Each entity's component count must match.
            for (const auto& entity : decodeResult.decoded.entities)
            {
                if (entity.isTombstone)
                    continue;
                if (entity.components.size() > kMaxComponentsPerEntity)
                {
                    ++result.failures;
                    result.lastFailureInput = HexDump(payload);
                    result.errorMessage = "component count exceeds max";
                    result.success = false;
                    break;
                }
            }
            if (!result.success)
                break;
        }
        // If decode fails, that is expected for random garbage — no error.
    }

    result.success = (result.failures == 0);
    return result;
}

// ─── Fuzz snapshot wire decoder ────────────────────────────────────────────

FuzzTestResult FuzzSnapshotWireDecoder(std::uint64_t iterations, std::uint64_t seed)
{
    FuzzTestResult result;
    result.seed = seed;

    if (seed == 0)
    {
        std::random_device rd;
        result.seed = rd();
    }

    FuzzRng rng(result.seed);

    for (std::uint64_t i = 0; i < iterations; ++i)
    {
        ++result.iterations;

        std::size_t payloadSize = rng.Bytes(0, 1024).size();
        std::vector<std::uint8_t> payload = rng.Bytes(0, payloadSize);

        SnapshotDecodeResult decodeResult = DecodeSnapshotWire(
            payload.data(), payload.size(), 1024);

        if (decodeResult.success)
        {
            // Entity count must match.
            if (decodeResult.decoded.entities.size() != decodeResult.decoded.entityCount)
            {
                ++result.failures;
                result.lastFailureInput = HexDump(payload);
                result.errorMessage = "entity count mismatch";
                result.success = false;
                break;
            }
        }
    }

    result.success = (result.failures == 0);
    return result;
}

// ─── Chaos packet ordering ──────────────────────────────────────────────────

FuzzTestResult ChaosPacketOrdering(std::uint64_t numDeltas, std::uint64_t dropRatePct, std::uint64_t seed)
{
    FuzzTestResult result;
    result.seed = seed;

    if (seed == 0)
    {
        std::random_device rd;
        result.seed = rd();
    }

    FuzzRng rng(result.seed);

    // Build a sequence of delta ticks.
    std::vector<std::uint64_t> ticks;
    for (std::uint64_t i = 0; i < numDeltas; ++i)
        ticks.push_back(i + 1);

    // Randomly drop some.
    std::vector<std::uint64_t> remaining;
    for (std::uint64_t t : ticks)
    {
        if (rng.Uint32() % 100 >= dropRatePct)
            remaining.push_back(t);
    }

    // Shuffle.
    std::shuffle(remaining.begin(), remaining.end(), rng.rng);

    // Simulate feeding them through the state machine.
    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    std::uint64_t accepted = 0;
    std::uint64_t dropped = 0;
    std::uint64_t baseline = 0;

    for (std::uint64_t t : remaining)
    {
        AcceptResult ar = sm.AcceptPacket(
            static_cast<ServerTick>(t), false /* delta */);

        switch (ar)
        {
            case AcceptResult::Accept:
                ++accepted;
                baseline = t;
                sm.RecordSequence(t);
                break;
            case AcceptResult::Buffer:
                // Buffered — valid.
                break;
            case AcceptResult::Drop:
                ++dropped;
                break;
        }

        sm.Tick(static_cast<ServerTick>(t));

        // If we entered Desynced, that is acceptable under chaos.
        // The test passes as long as the state machine does not crash.
    }

    result.success = true;
    result.iterations = remaining.size();
    return result;
}

// ─── Soak replication pipeline ──────────────────────────────────────────────

FuzzTestResult SoakReplicationPipeline(std::uint64_t tickCount, std::uint64_t seed)
{
    FuzzTestResult result;
    result.seed = seed;

    if (seed == 0)
    {
        std::random_device rd;
        result.seed = rd();
    }

    FuzzRng rng(result.seed);

    // Configure state machine.
    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Boot);

    std::uint64_t accepted = 0;
    std::uint64_t dropped = 0;

    for (std::uint64_t tick = 1; tick <= tickCount; ++tick)
    {
        ++result.iterations;

        // Simulate receiving a delta with some jitter.
        std::uint64_t seq = tick + (rng.Uint32() % 3);

        AcceptResult ar = sm.AcceptPacket(
            static_cast<ServerTick>(tick), false);

        switch (ar)
        {
            case AcceptResult::Accept:
                ++accepted;
                sm.RecordSequence(seq);
                sm.UpdateRtt(50.0f + static_cast<float>(rng.Uint32() % 100));
                break;
            case AcceptResult::Drop:
                ++dropped;
                break;
            case AcceptResult::Buffer:
                break;
        }

        sm.Tick(static_cast<ServerTick>(tick));

        // Periodically reset to test state transitions.
        if (tick % 10000 == 0 && sm.CurrentState() == ReplicationState::Synced)
        {
            sm.TransitionTo(ReplicationState::Desynced);
        }
    }

    result.success = true;
    return result;
}

} // namespace SagaEngine::Client::Replication
