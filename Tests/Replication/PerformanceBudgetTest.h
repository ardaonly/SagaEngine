/// @file PerformanceBudgetTest.h
/// @brief Performance budget enforcement tests for the replication pipeline.
///
/// Purpose: Verifies that the replication pipeline stays within defined
///          performance budgets under various load conditions.  Each test
///          measures a specific metric and compares it against the budget:
///            - Max apply time per tick: < 2ms
///            - Worst-case entity burst: < 10ms for 10000 entities
///            - Component deserialize budget: < 1μs per component
///            - Cold cache cost: first apply after reset

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Client::Replication {

struct PerformanceBudgetResult
{
    bool              withinBudget = false;
    std::uint64_t     measuredValue = 0;  ///< Microseconds or bytes.
    std::uint64_t     budgetLimit = 0;
    std::string       failureDetail;
};

/// Test max apply time per tick (< 2000 μs).
[[nodiscard]] PerformanceBudgetResult TestMaxApplyTimePerTick() noexcept;

/// Test worst-case entity burst cost (< 10000 μs for 10000 entities).
[[nodiscard]] PerformanceBudgetResult TestWorstCaseEntityBurst() noexcept;

/// Test component deserialize budget (< 1 μs per component).
[[nodiscard]] PerformanceBudgetResult TestComponentDeserializeBudget() noexcept;

/// Test cold cache cost (first apply after reset).
[[nodiscard]] PerformanceBudgetResult TestColdCacheCost() noexcept;

/// Run all performance budget tests.
[[nodiscard]] std::vector<PerformanceBudgetResult> RunAllPerformanceBudgetTests() noexcept;

} // namespace SagaEngine::Client::Replication
