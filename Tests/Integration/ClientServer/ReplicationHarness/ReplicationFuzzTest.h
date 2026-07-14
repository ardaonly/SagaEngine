/// @file ReplicationFuzzTest.h
/// @brief Fuzz and chaos test infrastructure for the replication pipeline.
///
/// Layer  : SagaEngine / Tests / Replication
/// Purpose: Generates randomised delta snapshots, corrupted payloads,
///          and out-of-order packet sequences to exercise the bounds
///          checks, state machine transitions, and recovery paths of
///          the client-side replication pipeline.  A replication
///          system that has not been fuzz-tested is not production-ready.
///
/// Design rules:
///   - Every fuzz test is deterministic given a seed, so failures
///     can be replayed exactly.
///   - Corrupted payloads test every bounds check individually.
///   - Chaos mode randomises packet order, drops, and duplicates.
///   - Soak tests run the pipeline for extended tick counts to
///     expose memory leaks and counter overflows.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Fuzz test result ──────────────────────────────────────────────────────

struct FuzzTestResult
{
    bool                success = false;
    std::uint64_t       iterations = 0;
    std::uint64_t       failures = 0;
    std::uint64_t       seed = 0;
    std::string         lastFailureInput;  ///< Hex dump of the failing input.
    std::string         errorMessage;
};

// ─── Fuzz test: wire decoder bounds ─────────────────────────────────────────

/// Fuzz the delta snapshot wire decoder with randomised payloads.
///
/// Generates payloads that exercise:
///   - Truncated headers
///   - Oversized entity counts
///   - Oversized component counts
///   - dataLen exceeding remaining bytes
///   - Invalid type IDs
///   - Valid payloads with random data
///
/// @param iterations  Number of random payloads to generate.
/// @param seed        PRNG seed for reproducibility.  Zero uses a random seed.
[[nodiscard]] FuzzTestResult FuzzDeltaWireDecoder(
    std::uint64_t iterations = 100000,
    std::uint64_t seed = 0);

/// Fuzz the full snapshot wire decoder with randomised payloads.
///
/// Generates payloads that exercise:
///   - Bad magic values
///   - Truncated headers
///   - CRC32 mismatches
///   - Oversized entity counts
///   - Truncated component data
[[nodiscard]] FuzzTestResult FuzzSnapshotWireDecoder(
    std::uint64_t iterations = 100000,
    std::uint64_t seed = 0);

// ─── Chaos test: packet ordering ───────────────────────────────────────────

/// Chaos test: feed deltas in random order with drops and duplicates.
///
/// Simulates network conditions that trigger:
///   - Out-of-order delivery
///   - Packet drops
///   - Duplicate packets
///   - Gap threshold breaches
///
/// The test verifies that the state machine transitions correctly
/// and that the pipeline never applies a delta to the wrong baseline.
[[nodiscard]] FuzzTestResult ChaosPacketOrdering(
    std::uint64_t numDeltas = 64,
    std::uint64_t dropRatePct = 10,
    std::uint64_t seed = 0);

// ─── Soak test: extended replication run ────────────────────────────────────

/// Soak test: run the replication pipeline for many ticks.
///
/// Exercises:
///   - Memory leak detection (bytes allocated == bytes freed)
///   - Counter overflow (all counters remain within bounds)
///   - Jitter buffer stability (no unbounded growth)
///   - State machine stability (no invalid transitions)
///
/// @param tickCount  Number of ticks to simulate (default: 100000).
/// @param seed       PRNG seed for reproducibility.
[[nodiscard]] FuzzTestResult SoakReplicationPipeline(
    std::uint64_t tickCount = 100000,
    std::uint64_t seed = 0);

} // namespace SagaEngine::Client::Replication
