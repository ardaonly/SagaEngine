/// @file TickDriftCorrector.h
/// @brief Client clock drift correction and long-term tick alignment.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Detects and corrects gradual drift between the client's local
///          tick clock and the server's authoritative tick clock.  Without
///          correction, even a 0.1% clock rate difference accumulates to
///          a full tick of error in ~15 minutes at 60 Hz, causing
///          interpolation jitter and prediction misalignment.
///
/// Design rules:
///   - Measures per-packet clock offset (server tick - client tick at
///     receive time) and applies exponential smoothing.
///   - Applies gradual correction rather than instant snapping to avoid
///     visible stutter.
///   - Reports cumulative drift so the caller can adjust the client's
///     tick rate (e.g., by skipping or duplicating frames).

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"

#include <cstdint>

namespace SagaEngine::Client::Replication {

// ─── Drift correction config ───────────────────────────────────────────────

struct TickDriftConfig
{
    /// Smoothing factor for the offset EMA (0.01-0.1 typical).
    /// Lower = smoother but slower to react.
    float smoothingAlpha = 0.02f;

    /// Maximum correction applied per tick (in client tick units).
    /// Limits visible stutter during correction.
    float maxCorrectionPerTick = 0.5f;

    /// Number of consecutive measurements required before correction
    /// begins.  Prevents reacting to transient jitter.
    std::uint32_t warmupSamples = 10;

    /// Drift threshold (in ticks).  Correction is only applied when
    /// the smoothed offset exceeds this value.
    float deadZoneTicks = 0.25f;
};

// ─── Drift measurement ─────────────────────────────────────────────────────

struct TickDriftState
{
    /// Smoothed clock offset (server tick - client tick) in ticks.
    /// Positive means the server is ahead; negative means behind.
    float smoothedOffsetTicks = 0.0f;

    /// Number of valid measurements collected since last reset.
    std::uint32_t sampleCount = 0;

    /// Whether the drift exceeds the dead zone and correction is active.
    bool correctionActive = false;

    /// Cumulative correction applied since session start (in ticks).
    float cumulativeCorrection = 0.0f;
};

// ─── Tick drift corrector ──────────────────────────────────────────────────

/// Measures and corrects clock drift between client and server.
class TickDriftCorrector
{
public:
    TickDriftCorrector() = default;
    ~TickDriftCorrector() = default;

    TickDriftCorrector(const TickDriftCorrector&)            = delete;
    TickDriftCorrector& operator=(const TickDriftCorrector&) = delete;

    /// Configure the corrector.  Must be called before use.
    void Configure(TickDriftConfig config) noexcept;

    /// Record a clock offset measurement.  Called each time a packet
    /// arrives from the server.
    ///
    /// @param serverTick  The server's tick stamped on the packet.
    /// @param clientTick  The client's current tick when the packet arrived.
    void RecordOffset(ServerTick serverTick, ServerTick clientTick) noexcept;

    /// Compute the correction to apply this frame.  Returns the number
    /// of client ticks to advance (may be fractional for interpolation).
    ///
    /// @returns Correction in client ticks (0.0 if no correction needed).
    [[nodiscard]] float ComputeCorrection() const noexcept;

    /// Return the current drift measurement.
    [[nodiscard]] TickDriftState GetState() const noexcept;

    /// Reset all accumulated drift.  Call on reconnect or resync.
    void Reset() noexcept;

private:
    TickDriftConfig config_{};
    TickDriftState  state_{};
};

} // namespace SagaEngine::Client::Replication
