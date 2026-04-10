/// @file ReplayGuard.h
/// @brief Per-connection replay-attack prevention via sliding sequence window
///        and monotonic timestamp validation.
///
/// Layer  : SagaServer / Networking / Security
/// Purpose: An attacker that captures a valid authenticated packet can
///          replay it later to trigger duplicate gameplay actions — a
///          classic exploit category for authoritative MMOs.  The defence
///          is two-layered:
///            1. Reject packets whose sequence number falls outside a
///               sliding "seen" window of recent sequences.
///            2. Reject packets whose wall-clock timestamp is older than
///               the last accepted packet minus a tolerance, or too far in
///               the future (clock skew bound).
///
/// Design:
///   - Sliding window is a 64-bit mask of "last N sequences accepted".  When
///     a new higher sequence arrives the mask is shifted; duplicates and
///     out-of-window old sequences are rejected.  64-bit is enough for a
///     typical tick-rate server — one second of traffic at 60 Hz fits twice.
///   - Timestamps are milliseconds since an epoch agreed at handshake;
///     this header only cares about ordering, not units.
///   - No heap allocation, no mutex — one instance per ServerConnection.
///     ServerConnection already serialises packet ingest on a single thread,
///     so the guard can be non-atomic.
///
/// Integration:
///   - Call `Accept(sequence, timestampMs)` on every incoming authenticated
///     packet, before dispatching to the packet registry.
///   - The returned `Decision` tells the caller whether to process, drop
///     silently (replay), or log+drop (clock-skew attack).

#pragma once

#include <cstdint>

namespace SagaEngine::Networking::Security
{

// ─── Decision ─────────────────────────────────────────────────────────────────

/// Result of a replay guard check.  The caller uses this to decide whether
/// to process the packet, silently drop it, or escalate to the security log.
enum class ReplayDecision : std::uint8_t
{
    /// Packet is fresh — process it normally.
    Accept = 0,

    /// Sequence has already been seen or is outside the sliding window.
    /// Drop silently — happens on normal packet duplication from UDP retry.
    DuplicateSequence = 1,

    /// Timestamp is older than the freshness threshold relative to the last
    /// accepted packet.  Likely a recorded replay.
    StaleTimestamp = 2,

    /// Timestamp is too far in the future — either severe clock skew or
    /// malicious forgery.  Escalate to the security log.
    FutureTimestamp = 3
};

// ─── ReplayGuard ──────────────────────────────────────────────────────────────

/// Single-connection replay validator.  NOT thread-safe by design — each
/// ServerConnection owns its own instance and mutates it only from the
/// packet-ingest thread.
class ReplayGuard
{
public:
    /// How far a packet's sequence is allowed to be behind the highest
    /// accepted sequence.  64 matches the bit width of the acceptance mask.
    static constexpr std::uint32_t kWindowSize = 64;

    /// Default staleness tolerance in milliseconds.  A packet whose
    /// timestamp is older than `lastAcceptedTimestamp - kStalenessMs` is
    /// treated as a replay.  2000 ms is a generous cushion for jitter.
    static constexpr std::int64_t kDefaultStalenessMs = 2000;

    /// Default forward-skew tolerance in milliseconds.  Packets claiming to
    /// be more than this far in the future of the server clock are rejected.
    static constexpr std::int64_t kDefaultFutureSkewMs = 500;

    ReplayGuard() noexcept = default;

    /// Override the freshness / future-skew tolerances.  Call once at
    /// construction time; changing them at runtime is not supported.
    void Configure(std::int64_t stalenessMs,
                   std::int64_t futureSkewMs) noexcept;

    /// Validate a new incoming packet.  `sequence` must be monotonically
    /// issued by the sender; `timestampMs` is in the shared epoch agreed at
    /// handshake; `serverNowMs` is the server's current wall-clock in the
    /// same epoch.
    [[nodiscard]] ReplayDecision Accept(std::uint64_t sequence,
                                        std::int64_t  timestampMs,
                                        std::int64_t  serverNowMs) noexcept;

    /// Reset all state — called when a connection is reused for a new session
    /// so stale window bits from the previous session cannot leak into the
    /// new one.
    void Reset() noexcept;

    // ── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] std::uint64_t HighestAcceptedSequence() const noexcept
    {
        return highestSeq_;
    }
    [[nodiscard]] std::uint64_t AcceptedCount() const noexcept { return accepted_; }
    [[nodiscard]] std::uint64_t RejectedCount() const noexcept { return rejected_; }

private:
    std::uint64_t windowMask_  = 0;   ///< Bit i set = "seq (highestSeq - i) accepted".
    std::uint64_t highestSeq_  = 0;   ///< Most recent accepted sequence number.
    std::int64_t  lastTimeMs_  = 0;   ///< Timestamp of the most recent accepted packet.

    std::int64_t  stalenessMs_  = kDefaultStalenessMs;
    std::int64_t  futureSkewMs_ = kDefaultFutureSkewMs;

    std::uint64_t accepted_ = 0;      ///< Diagnostics: total Accept results.
    std::uint64_t rejected_ = 0;      ///< Diagnostics: total non-Accept results.
};

} // namespace SagaEngine::Networking::Security
