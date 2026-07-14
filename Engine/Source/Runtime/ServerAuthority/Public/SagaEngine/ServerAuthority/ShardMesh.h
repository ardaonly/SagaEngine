/// @file ShardMesh.h
/// @brief Abstract shard-to-shard communication fabric.
///
/// Layer  : SagaServer / Networking / Server
/// Purpose: Every piece of cross-zone machinery we have — handoffs,
///          apron visibility, global chat, guild state — needs a way to
///          send a structured message from one zone process to another
///          zone process and have it delivered reliably, ordered, and
///          with a basic authenticity check.
///
///          Instead of each subsystem inventing its own TCP socket and
///          reconnect loop, the shard mesh exposes one uniform
///          interface and all of the above talk through it.  The
///          actual transport (gRPC, raw TCP, NATS, a shared-memory
///          fast path for zones co-located on one host) is a plugin
///          chosen at deployment time.
///
/// What "reliable" means here:
///   - In-order delivery between any two peers.  Gameplay depends on
///     Handoff → Commit ordering, so an unreliable UDP fabric is a
///     non-starter.
///   - Exactly-once semantics are NOT promised.  The mesh may deliver
///     a duplicate after a reconnect; every message type must carry a
///     `messageId` and the receiver deduplicates.
///   - No partition tolerance is promised.  If a peer is unreachable,
///     Send() returns an error and the caller must decide whether to
///     buffer, drop, or escalate.  The mesh is not a distributed queue.
///
/// Threading:
///   ShardMesh calls into receiver callbacks on an internal I/O thread.
///   Callbacks are expected to return promptly and push heavy work to
///   the zone's own task graph.  Implementations MUST serialise
///   callbacks on a per-peer basis so per-peer receive loops don't
///   have to worry about concurrent dispatch.

#pragma once

#include "SagaServer/Networking/Server/ConnectionMigration.h" // ZoneId

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Networking {

// ─── Peer identity ──────────────────────────────────────────────────────────

/// Identity of a mesh peer.  Combines the zone id (which process) with
/// a network endpoint (how to reach it).  Endpoint strings are opaque
/// to the mesh contract; each backend interprets them ("tcp://host:port",
/// "uds:///run/saga/zone-3.sock", etc.).
struct ShardPeerDescriptor
{
    ZoneId      zoneId = 0;
    std::string endpoint;
    std::string displayName; ///< Human-friendly, shows up in /meshstat.
};

// ─── Delivery results ───────────────────────────────────────────────────────

/// Return code on Send().  The caller branches on these; there is no
/// retry policy at the mesh layer because different callers want
/// different retry strategies.
enum class ShardMeshSendResult : std::uint8_t
{
    Ok = 0,
    PeerUnknown,          ///< No registered descriptor for this zoneId.
    PeerDisconnected,     ///< Known peer, but link is down right now.
    PayloadTooLarge,
    BackpressureFull,     ///< Per-peer outbound queue is saturated.
    InternalError,
};

// ─── Inbound handler ────────────────────────────────────────────────────────

/// Callback fired by the mesh when a message arrives.  The bytes are
/// owned by the mesh and remain valid only for the duration of the
/// call; handlers must copy if they need to retain anything.
using ShardMeshInboundHandler = std::function<void(
    ZoneId                    sourcePeer,
    const std::uint8_t*       payload,
    std::size_t               payloadSize)>;

/// Connection-state callback.  The mesh uses this to surface link-up
/// and link-down events; consumers can use it to pause apron updates
/// while a neighbour is bouncing.
enum class ShardPeerState : std::uint8_t { Disconnected, Connecting, Connected, Failed };
using ShardMeshPeerStateHandler = std::function<void(ZoneId peer, ShardPeerState state)>;

// ─── Interface ──────────────────────────────────────────────────────────────

/// Abstract mesh interface.  One instance per zone process.  Owned by
/// ZoneServer and handed to subsystems at boot via SubsystemManager.
class IShardMesh
{
public:
    virtual ~IShardMesh() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /// Initialise the mesh, open the listening endpoint, and connect to
    /// any peers provided at construction.  Non-reentrant — call exactly
    /// once from the owning service's OnInit.
    [[nodiscard]] virtual bool Start(const std::string& selfEndpoint) = 0;

    /// Tear down the mesh, close all sockets, drain in-flight callbacks.
    /// Safe to call even if Start() failed.
    virtual void Stop() = 0;

    // ── Peer registry ─────────────────────────────────────────────────────

    /// Register or update a peer descriptor.  The mesh will attempt to
    /// maintain a connection to any registered peer until it is
    /// explicitly removed.
    virtual void RegisterPeer(const ShardPeerDescriptor& desc) = 0;

    /// Remove a peer from the registry.  In-flight messages are
    /// abandoned; the caller must have already coordinated shutdown at
    /// the gameplay layer.
    virtual void UnregisterPeer(ZoneId peer) = 0;

    // ── Sending ───────────────────────────────────────────────────────────

    /// Enqueue a message to `peer`.  The payload is copied into the
    /// mesh's outbound buffer before the call returns, so the caller
    /// may free `bytes` immediately.
    [[nodiscard]] virtual ShardMeshSendResult Send(
        ZoneId              peer,
        const std::uint8_t* bytes,
        std::size_t         size) = 0;

    // ── Callbacks ─────────────────────────────────────────────────────────

    /// Install the inbound handler.  May be replaced at runtime — the
    /// mesh guarantees it will not invoke the old handler after the
    /// swap.  nullptr disables inbound delivery (bytes are dropped).
    virtual void SetInboundHandler(ShardMeshInboundHandler handler) = 0;

    /// Install the peer-state callback.  Optional; if null, state
    /// changes are logged but not dispatched.
    virtual void SetPeerStateHandler(ShardMeshPeerStateHandler handler) = 0;

    // ── Diagnostics ───────────────────────────────────────────────────────

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;

    /// Snapshot of per-peer counters.  Used by /meshstat and by an
    /// internal watchdog that trips when a link's queue depth keeps
    /// growing.  Implementations may return an empty vector if they do
    /// not collect stats.
    struct PeerStats
    {
        ZoneId        peer              = 0;
        ShardPeerState state            = ShardPeerState::Disconnected;
        std::uint64_t bytesSent         = 0;
        std::uint64_t bytesReceived     = 0;
        std::uint64_t messagesSent      = 0;
        std::uint64_t messagesReceived  = 0;
        std::uint32_t pendingOutbound   = 0;
    };
    [[nodiscard]] virtual std::vector<PeerStats> CollectStats() const = 0;
};

using ShardMeshPtr = std::shared_ptr<IShardMesh>;

// ─── Constants ──────────────────────────────────────────────────────────────

/// Per-peer outbound queue cap.  Above this depth Send() returns
/// BackpressureFull instead of growing the queue; the caller decides
/// whether to drop, coalesce, or escalate.
inline constexpr std::uint32_t kShardMeshOutboundQueueCap = 4096;

/// Hard ceiling on a single mesh message.  Matches the largest single
/// apron frame we will send.
inline constexpr std::uint32_t kShardMeshMaxMessageBytes = 256 * 1024;

} // namespace SagaEngine::Networking
