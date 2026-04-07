/// @file SimulationTick.cpp
/// @brief SimulationTick method implementations.

#include "SagaEngine/Simulation/SimulationTick.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>

namespace SagaEngine::Simulation {

// ─── Construction ──────────────────────────────────────────────────────────────

SimulationTick::SimulationTick(uint32_t fixedHz) noexcept
    : m_tickRate(fixedHz > 0 ? fixedHz : 64u)
    , m_fixedDelta(1.0 / static_cast<double>(m_tickRate > 0 ? m_tickRate : 64u))
    , m_startTime(std::chrono::steady_clock::now())
{
}

// ─── State ─────────────────────────────────────────────────────────────────────

float SimulationTick::Alpha() const noexcept
{
    return static_cast<float>(m_accumulator / m_fixedDelta);
}

// ─── Advance ───────────────────────────────────────────────────────────────────

uint32_t SimulationTick::Advance(double wallDeltaSeconds) noexcept
{
    if (wallDeltaSeconds <= 0.0) return 0u;

    m_accumulator += wallDeltaSeconds;

    // Spiral-of-death guard: clamp accumulator to at most kMaxCatchupTicks.
    const double maxAccumulator = m_fixedDelta * static_cast<double>(kMaxCatchupTicks);
    if (m_accumulator > maxAccumulator)
    {
        LOG_WARN("SimulationTick",
            "Accumulator clamped: %.4fs → %.4fs (tick rate=%u Hz). "
            "Frame was too slow — simulation may desync.",
            m_accumulator, maxAccumulator, m_tickRate);
        m_accumulator = maxAccumulator;
    }

    uint32_t ticks = 0u;
    while (m_accumulator >= m_fixedDelta)
    {
        m_accumulator -= m_fixedDelta;
        ++m_currentTick;
        ++ticks;
    }

    return ticks;
}

// ─── Server scheduling ─────────────────────────────────────────────────────────

std::chrono::steady_clock::time_point SimulationTick::NextTickDeadline() const noexcept
{
    // Absolute deadline: avoids cumulative drift from sleep imprecision.
    // nextTick is the tick we haven't started yet.
    const uint64_t nextTick = m_currentTick + 1u;
    const auto nsPerTick = static_cast<int64_t>(m_fixedDelta * 1'000'000'000.0);
    const auto offset = std::chrono::nanoseconds(nsPerTick * static_cast<int64_t>(nextTick));
    return m_startTime + offset;
}

// ─── Reset ─────────────────────────────────────────────────────────────────────

void SimulationTick::Reset() noexcept
{
    m_accumulator = 0.0;
    m_currentTick = 0u;
    m_startTime   = std::chrono::steady_clock::now();
}

} // namespace SagaEngine::Simulation
