/// @file ChaosTest.h
/// @brief Chaos testing harness for the replication pipeline.
///
/// Purpose: Exercises the replication system under adversarial network
///          conditions to verify robustness before production deployment.
///          Each chaos mode injects a specific class of failure:
///            - PacketReorder: shuffles delivery order
///            - RandomDrop: discards packets at configurable rate
///            - LatencySpike: adds artificial delay to subset of packets
///            - Corruption: flips random bytes in payloads
///            - GapStorm: generates large sequence gaps to trigger desync
///
/// Design rules:
///   - All chaos modes are deterministic given a seed for reproducibility.
///   - Each test runs for a configurable number of ticks.
///   - The harness verifies that the state machine never enters an
///     illegal state and that the pipeline recovers after chaos stops.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

// ─── Chaos mode ──────────────────────────────────────────────────────────────

enum class ChaosMode : std::uint8_t
{
    PacketReorder,     ///< Shuffles packet delivery order within a window.
    RandomDrop,        ///< Discards packets at a configurable rate.
    LatencySpike,      ///< Adds artificial delay to a subset of packets.
    Corruption,        ///< Flips random bytes in packet payloads.
    GapStorm,          ///< Generates large sequence gaps to trigger desync.
    Combined,          ///< All modes active simultaneously.
};

// ─── Chaos config ───────────────────────────────────────────────────────────

struct ChaosTestConfig
{
    ChaosMode mode = ChaosMode::PacketReorder;
    std::uint64_t seed = 0;
    std::uint64_t tickCount = 10000;
    float dropRate = 0.1f;           ///< For RandomDrop mode.
    float corruptionRate = 0.05f;    ///< For Corruption mode.
    std::uint32_t latencySpikeMs = 500; ///< For LatencySpike mode.
    std::uint64_t gapSize = 64;      ///< For GapStorm mode.
    std::uint32_t reorderWindow = 16; ///< For PacketReorder mode.
};

// ─── Chaos test result ──────────────────────────────────────────────────────

struct ChaosTestResult
{
    bool              success = false;
    std::uint64_t     ticksElapsed = 0;
    std::uint64_t     packetsInjected = 0;
    std::uint64_t     packetsDropped = 0;
    std::uint64_t     packetsCorrupted = 0;
    std::uint64_t     desyncEvents = 0;
    std::uint64_t     recoveryCount = 0;
    std::uint64_t     illegalStateTransitions = 0;
    std::string       errorMessage;
};

// ─── Test execution ─────────────────────────────────────────────────────────

/// Run a single chaos test with the given configuration.
///
/// @param config  Chaos mode and parameters.
/// @returns       Test result with pass/fail and diagnostics.
[[nodiscard]] ChaosTestResult RunChaosTest(ChaosTestConfig config) noexcept;

/// Run all chaos modes sequentially with default parameters.
///
/// @param tickCount  Number of ticks per mode.
/// @param seed       Base seed (each mode gets a derived seed).
/// @returns          Combined result across all modes.
[[nodiscard]] std::vector<ChaosTestResult> RunAllChaosModes(
    std::uint64_t tickCount = 10000,
    std::uint64_t seed = 42) noexcept;

} // namespace SagaEngine::Client::Replication
