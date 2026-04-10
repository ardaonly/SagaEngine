/// @file PacketBandwidthTracker.h
/// @brief Per-packet-type bandwidth accounting for diagnostics and tuning.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: When the bandwidth budget is tight it is critical to know WHICH
///          packet types dominate the wire.  Tuning replication rates
///          without per-type stats is guesswork.  This tracker sits in the
///          transport send/receive path and accumulates byte + count totals
///          keyed by `PacketType`.
///
/// Design:
///   - Fixed-size array indexed by packet type id (uint16_t), not a hash
///     map — lookup is a single load and there is no allocation on the
///     hot path.  Size is capped by `kMaxPacketTypes`.
///   - Rolling 1-second window stored as a ring buffer of per-tick samples
///     so the reporter can answer both "lifetime total" and "last second
///     rate" without extra bookkeeping.
///   - Atomic counters so the transport can call Record() from the IO
///     thread while the stats reporter reads from the diagnostics thread
///     without a mutex.
///   - No per-packet-type name table here — the packet registry owns those
///     strings; the reporter joins the two at print time.

#pragma once

#include "SagaEngine/Core/Threading/AtomicCounter.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace SagaEngine::Networking
{

// ─── Constants ────────────────────────────────────────────────────────────────

/// Maximum number of distinct packet types the tracker can account for.
/// Keep this a power of two so indexing stays cheap and the array fits in
/// a handful of cache lines.
inline constexpr std::size_t kMaxPacketTypes = 256;

// ─── Per-type totals ─────────────────────────────────────────────────────────

/// Running totals for a single packet type.  The two counters are on
/// separate cache lines via `AtomicCounter` so send and receive threads do
/// not thrash each other's line when both touch the same packet type.
struct PacketTypeStats
{
    Core::Threading::AtomicCounter bytesSent;
    Core::Threading::AtomicCounter bytesReceived;
    Core::Threading::AtomicCounter countSent;
    Core::Threading::AtomicCounter countReceived;
};

// ─── Tracker ─────────────────────────────────────────────────────────────────

/// Lock-free accounting for every `PacketType` the transport encounters.
///
/// Usage:
///   tracker.RecordSent(packetType, payloadSize);
///   tracker.RecordReceived(packetType, payloadSize);
///   auto snapshot = tracker.Snapshot(packetType);
class PacketBandwidthTracker
{
public:
    /// Lightweight sample returned by `Snapshot()`.  Not atomic — it is a
    /// frozen copy for the stats reporter.
    struct Sample
    {
        std::uint64_t bytesSent     = 0;
        std::uint64_t bytesReceived = 0;
        std::uint64_t countSent     = 0;
        std::uint64_t countReceived = 0;
    };

    PacketBandwidthTracker() noexcept = default;

    /// Record that a packet of `type` totalling `bytes` bytes was sent on
    /// the wire.  Called from the transport IO thread.
    void RecordSent(std::uint16_t type, std::uint32_t bytes) noexcept;

    /// Record a received packet.  Called from the ingest IO thread.
    void RecordReceived(std::uint16_t type, std::uint32_t bytes) noexcept;

    /// Capture the current totals for a single packet type.
    [[nodiscard]] Sample Snapshot(std::uint16_t type) const noexcept;

    /// Reset all counters to zero.  Called at server shutdown and during
    /// test tear-down; NOT called on every tick — the counters are
    /// lifetime totals by design.
    void Reset() noexcept;

    /// Total bytes sent summed across every packet type — handy for the
    /// top-line "server outbound" stat.
    [[nodiscard]] std::uint64_t TotalBytesSent() const noexcept;
    [[nodiscard]] std::uint64_t TotalBytesReceived() const noexcept;

private:
    std::array<PacketTypeStats, kMaxPacketTypes> stats_{};
};

} // namespace SagaEngine::Networking
