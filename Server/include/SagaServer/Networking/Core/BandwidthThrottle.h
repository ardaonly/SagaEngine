/// @file BandwidthThrottle.h
/// @brief Per-connection outbound bandwidth shaper (token bucket in bytes).
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: `RateLimiter` counts *packets* — that is enough to stop a peer
///          from flooding the accept loop but it does nothing about the
///          case a well-behaved client legitimately has a lot to send
///          during a raid and would otherwise blow past its slice of the
///          global uplink.  BandwidthThrottle counts *bytes* and is the
///          knob the replication and voice-chat layers use before they
///          hand a Packet to the transport.
///
/// Design:
///   - Classic byte-oriented token bucket: capacity = `burstBytes`,
///     refill = `bytesPerSecond` spread continuously over wall time.
///   - `TryConsume(bytes)` returns true iff the caller may send now; false
///     means "defer or drop".  Callers pick the policy — replication
///     defers by leaving the dirty bits marked, while pings drop silently.
///   - `OvershootBytes` tracks how many bytes the caller *asked for* past
///     the bucket; the replication graph uses this as an input signal to
///     step the client down one LOD tier.
///   - Thread-safe: a single `std::mutex` guards the small state.  Under
///     contention the critical section is ~40 ns, far cheaper than the
///     OS send() call that follows a successful consume.
///
/// Why separate from RateLimiter:
///   RateLimiter's semantics ("quarantine a peer for one second after N
///   offences") are a *protection* tool — it drops the connection if the
///   peer misbehaves.  BandwidthThrottle is a *cooperation* tool — it
///   asks well-behaved peers to slow down.  Mixing them in one class
///   conflates "refused" with "retry later".

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace SagaEngine::Networking {

// ─── Config ──────────────────────────────────────────────────────────────────

/// Constructor parameters.  Defaults track a per-client budget that fits
/// typical MMO PvE: 16 KiB/s sustained, 48 KiB burst for tick-boundary
/// snapshots.  Voice chat and large raid updates should raise these.
struct BandwidthThrottleConfig
{
    std::uint32_t bytesPerSecond = 16 * 1024;   ///< Refill rate, steady-state ceiling.
    std::uint32_t burstBytes     = 48 * 1024;   ///< Bucket capacity, short-term headroom.
};

// ─── Snapshot ────────────────────────────────────────────────────────────────

/// Read-only view returned by `Snapshot()`.  Exposes enough internal state
/// for dashboards and LOD decisions without letting callers poke values.
struct BandwidthThrottleStats
{
    std::uint64_t totalBytesConsumed = 0;  ///< Successful TryConsume byte total.
    std::uint64_t totalBytesRequested = 0; ///< Every TryConsume attempt, including rejected.
    std::uint64_t overshootBytes      = 0; ///< Sum of rejected request sizes.
    std::uint64_t rejections          = 0; ///< Number of rejected TryConsume calls.
    std::uint32_t currentTokens       = 0; ///< Last-known bucket fill.
};

// ─── BandwidthThrottle ───────────────────────────────────────────────────────

/// Per-connection byte-oriented token bucket.  One instance per
/// ServerConnection — never shared across clients.
class BandwidthThrottle
{
public:
    explicit BandwidthThrottle(BandwidthThrottleConfig cfg) noexcept;

    BandwidthThrottle(const BandwidthThrottle&)            = delete;
    BandwidthThrottle& operator=(const BandwidthThrottle&) = delete;
    BandwidthThrottle(BandwidthThrottle&&)                 = delete;
    BandwidthThrottle& operator=(BandwidthThrottle&&)      = delete;

    /// Attempt to reserve `bytes` from the bucket.  Returns true on
    /// success.  On failure the bucket is NOT drained — the caller may
    /// retry on the next tick without losing tokens to rejected tries.
    [[nodiscard]] bool TryConsume(std::uint32_t bytes);

    /// Refund previously consumed bytes.  Used when a scheduled packet
    /// was cancelled after the throttle already billed for it (for
    /// example when a zone transfer interrupts an in-flight snapshot).
    void Refund(std::uint32_t bytes);

    /// Hot-swap the sustained rate and burst ceiling.  Existing tokens
    /// are capped at the new `burstBytes`; refill continues from the
    /// current time.  Used by the shard LOD controller when a zone goes
    /// under heavy load and the server has to tighten everyone's budget.
    void Reconfigure(BandwidthThrottleConfig cfg) noexcept;

    /// Zero the statistics counters.  Does not change the bucket fill —
    /// the shaper behaviour is unaffected.  Intended for diagnostics
    /// dashboards that sample-and-clear on a fixed interval.
    void ResetStats() noexcept;

    /// Thread-safe snapshot of the counters.  Returned by value so the
    /// caller can log/ship it without holding our mutex.
    [[nodiscard]] BandwidthThrottleStats Snapshot() const;

    /// Convenience: bytes currently available without blocking.  Calls
    /// `Refill()` internally so the answer reflects wall clock, not the
    /// last observed state.
    [[nodiscard]] std::uint32_t AvailableTokens();

private:
    using Clock = std::chrono::steady_clock;

    /// Recompute the token count based on elapsed wall time.  Must be
    /// called with `mutex_` held.  Clamps the result to `burstBytes_`.
    void RefillLocked(Clock::time_point now) noexcept;

    mutable std::mutex mutex_;

    std::uint32_t     bytesPerSecond_;
    std::uint32_t     burstBytes_;
    double            tokens_;   ///< Fractional to avoid integer-division drift.
    Clock::time_point lastRefill_;

    // ── Stats (not hot) ────────────────────────────────────────────────────
    std::uint64_t totalBytesConsumed_  = 0;
    std::uint64_t totalBytesRequested_ = 0;
    std::uint64_t overshootBytes_      = 0;
    std::uint64_t rejections_          = 0;
};

} // namespace SagaEngine::Networking
