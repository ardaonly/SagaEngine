/// @file AdversarialModelTest.h
/// @brief Adversarial load model tests for the replication pipeline.
///
/// Purpose: Simulates malicious or anomalous client behavior to verify
///          the pipeline's resistance to exploitation.  Tests cover:
///            - CPU starvation: rapid packet flood to exhaust processing
///            - Gap storm: consecutive large sequence gaps to trigger desync
///            - Replay attack: re-sending old valid packets
///            - Sequence wrap-around: exploiting uint64 sequence overflow
///            - Decode bomb: oversized payloads designed to crash decoder

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct AdversarialTestResult
{
    bool              success = false;
    std::uint64_t     attackVectors = 0;
    std::uint64_t     attacksBlocked = 0;
    std::string       failureDetail;
};

/// Test CPU starvation resistance: rapid packet flood.
[[nodiscard]] AdversarialTestResult TestCpuStarvation(
    std::uint64_t seed = 42) noexcept;

/// Test gap storm resistance: consecutive large sequence gaps.
[[nodiscard]] AdversarialTestResult TestGapStorm(
    std::uint64_t seed = 42) noexcept;

/// Test replay attack resistance: re-sending old valid packets.
[[nodiscard]] AdversarialTestResult TestReplayAttack(
    std::uint64_t seed = 42) noexcept;

/// Test sequence wrap-around handling.
[[nodiscard]] AdversarialTestResult TestSequenceWrapAround(
    std::uint64_t seed = 42) noexcept;

/// Test decode bomb resistance: oversized payloads.
[[nodiscard]] AdversarialTestResult TestDecodeBomb(
    std::uint64_t seed = 42) noexcept;

/// Run all adversarial model tests.
[[nodiscard]] std::vector<AdversarialTestResult> RunAllAdversarialTests(
    std::uint64_t seed = 42) noexcept;

} // namespace SagaEngine::Client::Replication
