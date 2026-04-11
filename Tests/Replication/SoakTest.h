/// @file SoakTest.h
/// @brief Extended-duration soak tests for the replication pipeline.
///
/// Purpose: Runs the replication system for millions of ticks to expose
///          memory leaks, counter overflows, and slow state drift that
///          only manifest after hours of operation.  Each soak test
///          simulates a specific production scenario:
///            - NormalOperation: steady-state delta streaming
///            - IntermittentDesync: periodic desync + recovery cycles
///            - HighChurn: rapid entity spawn/despawn waves
///            - BandwidthPressure: sustained over-budget traffic

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct SoakTestResult
{
    bool              success = false;
    std::uint64_t     ticksElapsed = 0;
    std::uint64_t     peakMemoryBytes = 0;
    std::uint64_t     finalMemoryBytes = 0;
    std::uint64_t     totalDesyncs = 0;
    std::uint64_t     totalRecoveries = 0;
    std::string       failureDetail;
};

/// Run a normal-operation soak test for the specified tick count.
[[nodiscard]] SoakTestResult SoakNormalOperation(
    std::uint64_t tickCount = 1000000,
    std::uint64_t seed = 42) noexcept;

/// Run an intermittent-desync soak test.
[[nodiscard]] SoakTestResult SoakIntermittentDesync(
    std::uint64_t tickCount = 1000000,
    std::uint64_t seed = 42) noexcept;

/// Run a high-churn soak test with rapid spawn/despawn cycles.
[[nodiscard]] SoakTestResult SoakHighChurn(
    std::uint64_t tickCount = 1000000,
    std::uint64_t seed = 42) noexcept;

/// Run a bandwidth-pressure soak test.
[[nodiscard]] SoakTestResult SoakBandwidthPressure(
    std::uint64_t tickCount = 1000000,
    std::uint64_t seed = 42) noexcept;

/// Run all soak tests and return combined results.
[[nodiscard]] std::vector<SoakTestResult> RunAllSoakTests(
    std::uint64_t tickCount = 1000000,
    std::uint64_t seed = 42) noexcept;

} // namespace SagaEngine::Client::Replication
