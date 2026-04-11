/// @file TickDriftCorrector.cpp
/// @brief Client clock drift correction and long-term tick alignment.

#include "SagaEngine/Client/Replication/TickDriftCorrector.h"

#include <algorithm>
#include <cmath>

namespace SagaEngine::Client::Replication {

// ─── Configure ──────────────────────────────────────────────────────────────

void TickDriftCorrector::Configure(TickDriftConfig config) noexcept
{
    config_ = config;
}

// ─── Record offset ──────────────────────────────────────────────────────────

void TickDriftCorrector::RecordOffset(ServerTick serverTick, ServerTick clientTick) noexcept
{
    float rawOffset = static_cast<float>(static_cast<std::int64_t>(serverTick)
                                       - static_cast<std::int64_t>(clientTick));

    state_.sampleCount++;

    if (state_.sampleCount == 1)
    {
        // First sample — use it directly.
        state_.smoothedOffsetTicks = rawOffset;
    }
    else
    {
        // EMA smoothing.
        state_.smoothedOffsetTicks = config_.smoothingAlpha * rawOffset
                                   + (1.0f - config_.smoothingAlpha) * state_.smoothedOffsetTicks;
    }

    // Check if correction is needed.
    state_.correctionActive = std::abs(state_.smoothedOffsetTicks) > config_.deadZoneTicks;
}

// ─── Compute correction ─────────────────────────────────────────────────────

float TickDriftCorrector::ComputeCorrection() const noexcept
{
    if (!state_.correctionActive)
        return 0.0f;

    if (state_.sampleCount < config_.warmupSamples)
        return 0.0f;

    // Apply correction in the direction that reduces the offset,
    // but clamp to the maximum per-tick correction to avoid stutter.
    float correction = state_.smoothedOffsetTicks;
    float maxCorrection = config_.maxCorrectionPerTick;

    if (correction > maxCorrection)
        correction = maxCorrection;
    else if (correction < -maxCorrection)
        correction = -maxCorrection;

    return correction;
}

// ─── State ──────────────────────────────────────────────────────────────────

TickDriftState TickDriftCorrector::GetState() const noexcept
{
    return state_;
}

void TickDriftCorrector::Reset() noexcept
{
    state_ = {};
}

} // namespace SagaEngine::Client::Replication
