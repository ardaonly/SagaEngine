/// @file RateLimitGuard.h
/// @brief Adversarial load protection for the replication pipeline.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Detects and throttles malicious or anomalous packet patterns
///          including replay floods, rapid sequence skips, and sustained
///          decode failures.  Bounds checking protects against malformed
///          packets; rate limiting protects against valid-but-hostile
///          packet volumes.
///
/// Design rules:
///   - Token bucket for packet acceptance rate.
///   - Sequence skip anomaly detection: flags when the gap between
///     consecutive sequences exceeds the RTT-based expectation.
///   - Per-source decode failure counter: after a threshold, packets
///     from the source are dropped for a cool-down period.
///   - All rate limits are configurable at runtime.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace SagaEngine::Client::Replication {

// ─── Rate limit config ─────────────────────────────────────────────────────

struct RateLimitConfig
{
    /// Maximum packets accepted per second.  Excess packets are dropped.
    std::uint32_t maxPacketsPerSecond = 1000;

    /// Maximum consecutive decode failures before the source is
    /// quarantined for coolDownTicks.
    std::uint32_t maxConsecutiveDecodeFailures = 16;

    /// Number of ticks the source is quarantined after exceeding the
    /// decode failure threshold.
    std::uint32_t decodeFailureCoolDownTicks = 120;

    /// Maximum sequence gap that is accepted without flagging an anomaly.
    /// Gaps larger than this trigger an immediate alert.
    std::uint64_t maxSequenceGap = 256;

    /// Tokens added to the bucket per second (refill rate).
    std::uint32_t tokenRefillRate = 1000;

    /// Maximum tokens in the bucket (burst capacity).
    std::uint32_t tokenBucketCapacity = 2000;
};

// ─── Rate limit verdict ────────────────────────────────────────────────────

enum class RateLimitVerdict : std::uint8_t
{
    Accept,         ///< Packet is within rate limits.
    Throttle,       ///< Packet rate exceeded — drop silently.
    Quarantine,     ///< Source is quarantined — drop and log.
    AnomalyAlert,   ///< Packet accepted but flagged for review.
};

// ─── Rate limit guard ──────────────────────────────────────────────────────

/// Token-bucket-based rate limiter with anomaly detection.
///
/// Thread-safe: the accept check is atomic so it can be called from
/// the network thread before enqueue.  Configuration changes are
/// mutex-guarded.
class RateLimitGuard
{
public:
    RateLimitGuard() = default;
    ~RateLimitGuard() = default;

    RateLimitGuard(const RateLimitGuard&)            = delete;
    RateLimitGuard& operator=(const RateLimitGuard&) = delete;

    /// Configure the guard.  Must be called before use.
    void Configure(RateLimitConfig config) noexcept;

    /// Check whether a packet should be accepted based on the current
    /// rate limit state.  Call from the network thread (before enqueue).
    [[nodiscard]] RateLimitVerdict AcceptPacket() noexcept;

    /// Record a successful decode.  Resets the consecutive failure counter.
    void RecordDecodeSuccess() noexcept;

    /// Record a failed decode.  May trigger quarantine.
    [[nodiscard]] RateLimitVerdict RecordDecodeFailure() noexcept;

    /// Record a sequence gap.  May trigger an anomaly alert.
    [[nodiscard]] RateLimitVerdict RecordSequenceGap(std::uint64_t gap) noexcept;

    /// Tick the rate limiter.  Refills tokens and manages quarantine timers.
    void Tick(std::uint64_t currentTick, float dtSeconds) noexcept;

    /// Return the current number of available tokens.
    [[nodiscard]] std::uint32_t AvailableTokens() const noexcept;

    /// Return whether the source is currently quarantined.
    [[nodiscard]] bool IsQuarantined() const noexcept;

private:
    RateLimitConfig config_{};

    // Token bucket (atomic for lock-free network thread access).
    std::atomic<std::uint32_t> tokens_;

    // Decode failure tracking.
    std::atomic<std::uint32_t> consecutiveDecodeFailures_{0};
    std::atomic<bool>          quarantined_{false};
    std::uint64_t              quarantineUntilTick_ = 0;

    // Sequence gap tracking.
    std::uint64_t              lastSequence_ = 0;
    std::uint32_t              anomalyCount_ = 0;
};

} // namespace SagaEngine::Client::Replication
