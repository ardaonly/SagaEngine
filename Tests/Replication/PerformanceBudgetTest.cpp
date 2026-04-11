/// @file PerformanceBudgetTest.cpp
/// @brief Performance budget enforcement tests for the replication pipeline.

#include "PerformanceBudgetTest.h"

#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Client/Replication/RateLimitGuard.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"

#include <chrono>
#include <random>
#include <vector>

namespace SagaEngine::Client::Replication {

namespace {

/// High-resolution timer for microsecond measurements.
struct MicroTimer
{
    using Clock = std::chrono::high_resolution_clock;
    using Microseconds = std::chrono::microseconds;

    void Start() { start_ = Clock::now(); }

    std::uint64_t ElapsedUs() const {
        auto elapsed = Clock::now() - start_;
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<Microseconds>(elapsed).count());
    }

private:
    Clock::time_point start_;
};

/// Generate a synthetic delta payload with the given entity count.
std::vector<std::uint8_t> MakeDeltaPayload(std::uint32_t entityCount)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(entityCount * 40);

    for (std::uint32_t e = 0; e < entityCount; ++e)
    {
        std::uint32_t entityId = 1000 + e;
        std::uint16_t compCount = 3;

        payload.push_back(static_cast<std::uint8_t>(entityId));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 8));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 16));
        payload.push_back(static_cast<std::uint8_t>(entityId >> 24));
        payload.push_back(static_cast<std::uint8_t>(compCount));
        payload.push_back(static_cast<std::uint8_t>(compCount >> 8));

        for (int c = 0; c < compCount; ++c)
        {
            payload.push_back(static_cast<std::uint8_t>(c + 1));
            payload.push_back(0);
            payload.push_back(16); payload.push_back(0);  // 16 bytes per component
            for (int b = 0; b < 16; ++b)
                payload.push_back(static_cast<std::uint8_t>(e + c + b));
        }
    }

    return payload;
}

} // namespace

// ─── Max apply time per tick ────────────────────────────────────────────────

PerformanceBudgetResult TestMaxApplyTimePerTick() noexcept
{
    PerformanceBudgetResult result;
    result.budgetLimit = 2000;  // 2000 μs = 2ms

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});
    sm.TransitionTo(ReplicationState::Synced);

    MicroTimer timer;
    timer.Start();

    // Process 1000 ticks.
    for (std::uint64_t tick = 1; tick <= 1000; ++tick)
    {
        sm.RecordSequence(tick);
        sm.UpdateRtt(50.0f);
        sm.AcceptPacket(static_cast<ServerTick>(tick), false);
        sm.Tick(static_cast<ServerTick>(tick));
    }

    result.measuredValue = timer.ElapsedUs();
    result.withinBudget = (result.measuredValue < result.budgetLimit * 10);  // 10 ticks budget

    if (!result.withinBudget)
        result.failureDetail = "Apply time " + std::to_string(result.measuredValue) +
            " μs exceeded budget " + std::to_string(result.budgetLimit * 10) + " μs";

    return result;
}

// ─── Worst-case entity burst ────────────────────────────────────────────────

PerformanceBudgetResult TestWorstCaseEntityBurst() noexcept
{
    PerformanceBudgetResult result;
    result.budgetLimit = 10000;  // 10ms for 10000 entities

    auto payload = MakeDeltaPayload(10000);

    MicroTimer timer;
    timer.Start();

    DeltaDecodeResult decoded = DecodeDeltaWire(payload.data(), payload.size(), 1024);

    result.measuredValue = timer.ElapsedUs();
    result.withinBudget = (result.measuredValue < result.budgetLimit);

    if (!decoded.success)
    {
        result.failureDetail = "Decode failed for 10000 entities: " + decoded.error;
        return result;
    }

    if (!result.withinBudget)
        result.failureDetail = "Decode time " + std::to_string(result.measuredValue) +
            " μs exceeded budget " + std::to_string(result.budgetLimit) + " μs";

    return result;
}

// ─── Component deserialize budget ───────────────────────────────────────────

PerformanceBudgetResult TestComponentDeserializeBudget() noexcept
{
    PerformanceBudgetResult result;

    // Decode 1000 entities with 3 components each.
    auto payload = MakeDeltaPayload(1000);

    MicroTimer timer;
    timer.Start();

    DeltaDecodeResult decoded = DecodeDeltaWire(payload.data(), payload.size(), 1024);

    std::uint64_t elapsed = timer.ElapsedUs();
    std::uint64_t componentCount = decoded.success ? decoded.decoded.entities.size() * 3 : 0;

    result.budgetLimit = 1;  // 1 μs per component.
    result.measuredValue = componentCount > 0 ? elapsed / componentCount : 0;
    result.withinBudget = (result.measuredValue <= result.budgetLimit);

    if (!decoded.success)
    {
        result.failureDetail = "Decode failed: " + decoded.error;
        return result;
    }

    if (!result.withinBudget)
        result.failureDetail = "Per-component decode time " +
            std::to_string(result.measuredValue) + " μs exceeded budget";

    return result;
}

// ─── Cold cache cost ────────────────────────────────────────────────────────

PerformanceBudgetResult TestColdCacheCost() noexcept
{
    PerformanceBudgetResult result;
    result.budgetLimit = 5000;  // 5ms for cold start

    ReplicationStateMachine sm;
    sm.Configure({300, 2, 32});

    MicroTimer timer;
    timer.Start();

    // First tick after configuration (cold start).
    sm.TransitionTo(ReplicationState::Synced);
    sm.RecordSequence(1);
    sm.UpdateRtt(50.0f);
    sm.AcceptPacket(1, false);
    sm.Tick(1);

    result.measuredValue = timer.ElapsedUs();
    result.withinBudget = (result.measuredValue < result.budgetLimit);

    if (!result.withinBudget)
        result.failureDetail = "Cold start time " + std::to_string(result.measuredValue) +
            " μs exceeded budget " + std::to_string(result.budgetLimit) + " μs";

    return result;
}

// ─── Run all tests ──────────────────────────────────────────────────────────

std::vector<PerformanceBudgetResult> RunAllPerformanceBudgetTests() noexcept
{
    std::vector<PerformanceBudgetResult> results;
    results.reserve(4);

    results.push_back(TestMaxApplyTimePerTick());
    results.push_back(TestWorstCaseEntityBurst());
    results.push_back(TestComponentDeserializeBudget());
    results.push_back(TestColdCacheCost());

    return results;
}

} // namespace SagaEngine::Client::Replication
