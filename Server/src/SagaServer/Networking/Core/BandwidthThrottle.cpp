/// @file BandwidthThrottle.cpp
/// @brief Implementation of the per-connection byte token bucket.

#include "SagaServer/Networking/Core/BandwidthThrottle.h"

#include <algorithm>

namespace SagaEngine::Networking {

// ─── Construction ────────────────────────────────────────────────────────────

BandwidthThrottle::BandwidthThrottle(BandwidthThrottleConfig cfg) noexcept
    : bytesPerSecond_(cfg.bytesPerSecond)
    , burstBytes_(cfg.burstBytes)
    , tokens_(static_cast<double>(cfg.burstBytes)) // Start full so a fresh
                                                    // connection is not
                                                    // instantly throttled.
    , lastRefill_(Clock::now())
{
}

// ─── Refill ──────────────────────────────────────────────────────────────────

void BandwidthThrottle::RefillLocked(Clock::time_point now) noexcept
{
    // Refill proportional to elapsed wall time.  Using a fractional
    // `tokens_` value avoids the "accumulate 0.9 bytes and lose it to
    // integer truncation on every tick" drift that would otherwise bite
    // a rate of, say, 17 bytes/ms being sampled every 3 ms.
    const auto elapsed = now - lastRefill_;
    const double seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();

    if (seconds > 0.0)
    {
        tokens_ += seconds * static_cast<double>(bytesPerSecond_);
        if (tokens_ > static_cast<double>(burstBytes_))
            tokens_ = static_cast<double>(burstBytes_);
        lastRefill_ = now;
    }
}

// ─── TryConsume ──────────────────────────────────────────────────────────────

bool BandwidthThrottle::TryConsume(std::uint32_t bytes)
{
    std::lock_guard<std::mutex> lock(mutex_);

    RefillLocked(Clock::now());
    totalBytesRequested_ += bytes;

    // Reject oversize requests fast — they would starve the bucket
    // forever otherwise.  The caller can respond by splitting the
    // payload (FragmentAssembler) or by skipping this tick.
    if (static_cast<double>(bytes) > tokens_)
    {
        overshootBytes_ += bytes;
        rejections_     += 1;
        return false;
    }

    tokens_ -= static_cast<double>(bytes);
    totalBytesConsumed_ += bytes;
    return true;
}

// ─── Refund ──────────────────────────────────────────────────────────────────

void BandwidthThrottle::Refund(std::uint32_t bytes)
{
    std::lock_guard<std::mutex> lock(mutex_);

    tokens_ += static_cast<double>(bytes);
    if (tokens_ > static_cast<double>(burstBytes_))
        tokens_ = static_cast<double>(burstBytes_);

    // Deliberately don't touch totalBytesConsumed_.  The stat is a
    // historical record of "how much did this connection ask to send"
    // and a refund is not an unsend.
}

// ─── Reconfigure ─────────────────────────────────────────────────────────────

void BandwidthThrottle::Reconfigure(BandwidthThrottleConfig cfg) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    bytesPerSecond_ = cfg.bytesPerSecond;
    burstBytes_     = cfg.burstBytes;

    // Clamp the current token count to the new bucket ceiling so a
    // downshift takes effect immediately instead of after the next
    // refill.
    if (tokens_ > static_cast<double>(burstBytes_))
        tokens_ = static_cast<double>(burstBytes_);
}

// ─── ResetStats ──────────────────────────────────────────────────────────────

void BandwidthThrottle::ResetStats() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    totalBytesConsumed_  = 0;
    totalBytesRequested_ = 0;
    overshootBytes_      = 0;
    rejections_          = 0;
}

// ─── Snapshot ────────────────────────────────────────────────────────────────

BandwidthThrottleStats BandwidthThrottle::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    BandwidthThrottleStats out;
    out.totalBytesConsumed  = totalBytesConsumed_;
    out.totalBytesRequested = totalBytesRequested_;
    out.overshootBytes      = overshootBytes_;
    out.rejections          = rejections_;
    out.currentTokens       = static_cast<std::uint32_t>(tokens_);
    return out;
}

// ─── AvailableTokens ─────────────────────────────────────────────────────────

std::uint32_t BandwidthThrottle::AvailableTokens()
{
    std::lock_guard<std::mutex> lock(mutex_);
    RefillLocked(Clock::now());
    return static_cast<std::uint32_t>(tokens_);
}

} // namespace SagaEngine::Networking
