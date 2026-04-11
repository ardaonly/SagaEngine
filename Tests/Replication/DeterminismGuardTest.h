/// @file DeterminismGuardTest.h
/// @brief Cross-platform determinism verification tests.
///
/// Purpose: Verifies that the replication pipeline produces identical
///          results regardless of execution order, compiler optimisation
///          level, or platform architecture.  Each test exercises a
///          specific determinism invariant:
///            - Entity iteration order is ascending by EntityId.
///            - Component apply order is ascending by ComponentTypeId.
///            - Float canonicalisation eliminates NaN/denormal divergence.
///            - Granular hash matches between client and server states.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct DeterminismTestResult
{
    bool              success = false;
    std::uint64_t     iterations = 0;
    std::string       failureDetail;
};

/// Verify that two WorldStates produce identical granular hashes.
[[nodiscard]] DeterminismTestResult TestGranularHashConsistency(
    std::uint64_t seed = 42) noexcept;

/// Verify that component apply order produces identical results
/// regardless of input ordering.
[[nodiscard]] DeterminismTestResult TestApplyOrderIndependence(
    std::uint64_t seed = 42) noexcept;

/// Verify that float canonicalisation eliminates divergence.
[[nodiscard]] DeterminismTestResult TestFloatCanonicalisation(
    std::uint64_t seed = 42) noexcept;

/// Verify that entity iteration is deterministic across multiple runs.
[[nodiscard]] DeterminismTestResult TestEntityIterationDeterminism(
    std::uint64_t seed = 42) noexcept;

/// Run all determinism tests and return combined result.
[[nodiscard]] std::vector<DeterminismTestResult> RunAllDeterminismTests(
    std::uint64_t seed = 42) noexcept;

} // namespace SagaEngine::Client::Replication
