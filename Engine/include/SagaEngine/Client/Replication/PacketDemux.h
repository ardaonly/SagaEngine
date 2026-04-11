/// @file PacketDemux.h
/// @brief Network-to-pipeline bridge: demuxes replication packets.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Receives raw packets from the network layer (reliable channel),
///          strips the PacketHeader, routes by PacketType, performs bounds
///          validation, decodes wire format, and hands structured snapshots
///          to the state machine and pipeline.  This is the single entry
///          point for all replication data on the client side.
///
/// Thread safety: `Enqueue` is safe from the network thread (lock-free
/// SPSC).  `ProcessQueue` must be called from the game thread — it
/// drains the queue, decodes, and feeds the state machine + pipeline.

#pragma once

#include "SagaEngine/Client/Replication/SnapshotApplyPipeline.h"
#include "SagaEngine/Client/Replication/ReplicationStateMachine.h"
#include "SagaEngine/Client/Replication/DeltaSnapshotWire.h"
#include "SagaEngine/Client/Replication/WorldSnapshotWire.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>

namespace SagaEngine::Client::Replication {

// ─── Demux config ──────────────────────────────────────────────────────────

struct PacketDemuxConfig
{
    /// Capacity of the SPSC ring buffer (must be power of two).
    std::uint32_t queueCapacity = 128;

    /// Maximum packet payload size (256 KiB sanity limit).
    std::uint32_t maxPayloadSize = 256 * 1024;

    /// Handshake protocol version supported by this client.
    std::uint32_t clientProtocolVersion = 1;

    /// Handshake schema version supported by this client.
    std::uint32_t clientSchemaVersion = 1;
};

// ─── Demuxed packet ────────────────────────────────────────────────────────

/// A packet stripped of its network header, ready for decoding.
struct DemuxedPacket
{
    std::uint16_t packetType = 0;          ///< PacketType enum value.
    std::uint64_t sequence   = 0;          ///< Reliable channel sequence.
    std::uint64_t recvTick   = 0;          ///< Client tick when received.
    std::uint32_t payloadSize = 0;
    std::uint8_t  payload[256 * 1024]{};   ///< Inline storage (bounded).
};

// ─── Demux stats ───────────────────────────────────────────────────────────

struct PacketDemuxStats
{
    std::uint64_t packetsEnqueued   = 0;
    std::uint64_t packetsDequeued   = 0;
    std::uint64_t packetsDropped    = 0;  ///< Queue overflow or bounds fail.
    std::uint64_t decodeFailures    = 0;  ///< Wire format parse errors.
    std::uint64_t boundsViolations  = 0;  ///< Sanity limit breaches.
    std::uint32_t queueDepth        = 0;
};

// ─── Callbacks ─────────────────────────────────────────────────────────────

/// Invoked when a decoded full snapshot is ready for the pipeline.
using OnFullSnapshotFn = std::function<void(DecodedWorldSnapshot&&)>;

/// Invoked when a decoded delta snapshot is ready for the pipeline.
using OnDeltaSnapshotFn = std::function<void(DecodedDeltaSnapshot&&)>;

/// Invoked when a packet fails bounds checking (exploit attempt or corruption).
using OnBoundsViolationFn = std::function<void(const char* reason)>;

// ─── Packet demux ──────────────────────────────────────────────────────────

/// Bounded SPSC ring buffer for network → game thread handoff.
///
/// Network thread calls `Enqueue`.  Game thread calls `ProcessQueue`
/// which drains, decodes, and dispatches.  Overflow drops the oldest
/// entry (back-pressure on the network layer is the caller's job).
class PacketDemux
{
public:
    PacketDemux() = default;
    ~PacketDemux() = default;

    PacketDemux(const PacketDemux&)            = delete;
    PacketDemux& operator=(const PacketDemux&) = delete;

    /// Configure the demux.  Must be called before use.
    void Configure(PacketDemuxConfig config = {}) noexcept;

    // ─── Network thread (single-producer) ────────────────────────────────

    /// Enqueue a raw packet from the reliable channel.
    /// Lock-free, wait-free, safe from the network thread.
    /// Returns false if the queue is full (packet dropped).
    [[nodiscard]] bool Enqueue(
        std::uint16_t  packetType,
        std::uint64_t  sequence,
        const std::uint8_t* payload,
        std::uint32_t  payloadSize,
        std::uint64_t  recvTick) noexcept;

    // ─── Game thread (single-consumer) ───────────────────────────────────

    /// Install callbacks for decoded snapshots and bounds violations.
    void SetCallbacks(
        OnFullSnapshotFn    onFull,
        OnDeltaSnapshotFn   onDelta,
        OnBoundsViolationFn onViolation) noexcept;

    /// Set the state machine for packet acceptance decisions.
    void SetStateMachine(ReplicationStateMachine* sm) noexcept;

    /// Drain the queue, decode packets, run bounds checks, and
    /// dispatch to callbacks.  Call once per game frame.
    void ProcessQueue(ServerTick currentTick) noexcept;

    [[nodiscard]] PacketDemuxStats Stats() const noexcept;

private:
    PacketDemuxConfig config_{};

    // SPSC ring buffer fields.
    std::array<DemuxedPacket, 128> ring_{};
    std::atomic<std::uint32_t>     head_{0};  ///< Network thread writes here.
    std::uint32_t                  tail_{0};   ///< Game thread reads here.

    // Callbacks (game thread only).
    OnFullSnapshotFn    onFull_;
    OnDeltaSnapshotFn   onDelta_;
    OnBoundsViolationFn onViolation_;

    // State machine (game thread only).
    ReplicationStateMachine* stateMachine_ = nullptr;

    PacketDemuxStats stats_{};

    // Internal decode + dispatch for one packet.
    void DecodeAndDispatch(const DemuxedPacket& pkt, ServerTick currentTick) noexcept;
};

} // namespace SagaEngine::Client::Replication
