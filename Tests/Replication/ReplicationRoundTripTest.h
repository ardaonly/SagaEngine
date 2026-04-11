/// @file ReplicationRoundTripTest.h
/// @brief Round-trip tests for the replication pipeline: encode -> decode -> verify.
///
/// Purpose: Verifies that delta snapshots and full snapshots survive the
///          full encode-decode cycle without data loss or corruption.
///          Each test exercises a specific aspect of the wire format:
///            - DeltaSnapshot: entity spawn, update, despawn
///            - WorldSnapshot: full state serialization
///            - CRC32: integrity verification
///            - Schema version: forward-compatible type skipping

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct RoundTripResult
{
    bool              success = false;
    std::size_t       bytesEncoded = 0;
    std::size_t       bytesDecoded = 0;
    std::string       failureDetail;
};

/// Test delta snapshot round-trip: encode entities -> decode -> verify.
[[nodiscard]] RoundTripResult TestDeltaRoundTrip(
    std::uint32_t entityCount = 100,
    std::uint64_t seed = 42) noexcept;

/// Test full snapshot round-trip: serialize world -> deserialize -> verify.
[[nodiscard]] RoundTripResult TestSnapshotRoundTrip(
    std::uint32_t entityCount = 100,
    std::uint64_t seed = 42) noexcept;

/// Test CRC32 integrity verification on corrupted payloads.
[[nodiscard]] RoundTripResult TestCrcIntegrity(
    std::uint64_t seed = 42) noexcept;

/// Test schema version forward compatibility.
[[nodiscard]] RoundTripResult TestSchemaForwardCompat(
    std::uint64_t seed = 42) noexcept;

/// Run all round-trip tests.
[[nodiscard]] std::vector<RoundTripResult> RunAllRoundTripTests(
    std::uint64_t seed = 42) noexcept;

} // namespace SagaEngine::Client::Replication
