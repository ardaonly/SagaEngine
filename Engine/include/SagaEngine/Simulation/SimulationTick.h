/// @file SimulationTick.h
/// @brief Fixed-rate simulation tick accumulator with drift-free scheduling.
///
/// SimulationTick decouples wall-clock time from simulation time. It accumulates
/// wall-clock delta time and advances the simulation in discrete, fixed-size steps.
///
/// Why fixed timestep:
///   - Determinism: every tick is exactly kFixedDelta seconds of simulation time.
///   - Predictability: physics and movement integrate cleanly without variable dt.
///   - Networked play: client and server tick in lockstep — the same tick number
///     means the same moment in simulation time, regardless of frame rate.
///
/// Drift prevention:
///   Naive sleep_for(fixedDelta) drifts over time. Instead, the server computes
///   the absolute wall-clock deadline for each tick:
///     nextDeadline = startTime + tickNumber * fixedDelta
///   and sleeps until that deadline. Any overshoot is absorbed by the next tick.
///
/// Spiral of death guard:
///   If the simulation falls behind by more than kMaxCatchupTicks ticks in one
///   Advance() call, the accumulator is clamped. This prevents a slow frame from
///   spawning hundreds of ticks that make the next frame slower, ad infinitum.

#pragma once

#include <chrono>
#include <cstdint>

namespace SagaEngine::Simulation {

/// Fixed-rate tick accumulator.
class SimulationTick
{
public:
    /// @param fixedHz  Tick rate in Hz (default 64).
    explicit SimulationTick(uint32_t fixedHz = 64u) noexcept;

    // ─── Configuration ─────────────────────────────────────────────────────────

    /// Return the fixed delta time in seconds (1 / Hz).
    [[nodiscard]] double FixedDelta() const noexcept { return m_fixedDelta; }

    /// Return the tick rate in Hz.
    [[nodiscard]] uint32_t TickRate() const noexcept { return m_tickRate; }

    // ─── State ─────────────────────────────────────────────────────────────────

    /// Return the number of complete ticks simulated so far.
    [[nodiscard]] uint64_t CurrentTick() const noexcept { return m_currentTick; }

    /// Return accumulated but not-yet-simulated time in seconds.
    [[nodiscard]] double Accumulator() const noexcept { return m_accumulator; }

    /// Return the interpolation alpha in [0, 1] for rendering between ticks.
    ///
    /// alpha = accumulator / fixedDelta
    /// Use this to smoothly interpolate rendered positions between simulation ticks.
    [[nodiscard]] float Alpha() const noexcept;

    // ─── Advance ───────────────────────────────────────────────────────────────

    /// Accumulate wall-clock delta time and return how many ticks are ready.
    ///
    /// Call this once per frame with the elapsed wall-clock time since last call.
    /// The caller must execute exactly that many simulation ticks.
    ///
    /// Example:
    ///   const uint32_t ticks = simTick.Advance(dt);
    ///   for (uint32_t i = 0; i < ticks; ++i)
    ///       authority.Tick(simTick.CurrentTick() - ticks + i, inputEntries);
    [[nodiscard]] uint32_t Advance(double wallDeltaSeconds) noexcept;

    // ─── Server scheduling ─────────────────────────────────────────────────────

    /// Return the wall-clock deadline for the next tick.
    ///
    /// The server sleep loop uses this to schedule tick boundaries:
    ///   std::this_thread::sleep_until(simTick.NextTickDeadline());
    [[nodiscard]] std::chrono::steady_clock::time_point NextTickDeadline() const noexcept;

    // ─── Reset ─────────────────────────────────────────────────────────────────

    /// Reset tick counter and accumulator to zero. Resets the start time.
    void Reset() noexcept;

private:
    uint32_t m_tickRate;       ///< Ticks per second.
    double   m_fixedDelta;     ///< Seconds per tick = 1.0 / m_tickRate.
    double   m_accumulator{0}; ///< Accumulated unprocessed seconds.
    uint64_t m_currentTick{0}; ///< Total ticks completed.

    /// Absolute start time. Used by NextTickDeadline() for drift-free scheduling.
    std::chrono::steady_clock::time_point m_startTime;

    /// Maximum ticks emitted by one Advance() call (spiral-of-death guard).
    static constexpr uint32_t kMaxCatchupTicks = 8u;
};

} // namespace SagaEngine::Simulation
