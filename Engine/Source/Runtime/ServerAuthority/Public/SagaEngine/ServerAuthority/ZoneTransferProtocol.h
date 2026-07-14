/// @file ZoneTransferProtocol.h
/// @brief Shard-mesh wire format for cross-zone entity handoff.
///
/// Layer  : SagaServer / Networking / Server
/// Purpose: `ConnectionMigration.h` describes the *client-facing* side of
///          the handoff dance — what the player's connection sees, which
///          token they carry, which endpoint they reconnect to.  That is
///          only half the story.  The other half is the *shard-mesh*
///          wire protocol the source and destination zone services use
///          to actually move entity state between processes.
///
///          This header defines that mesh-side protocol.  It is
///          intentionally separate from ConnectionMigration so the two
///          evolve independently: a client-visible change (new field in
///          ZoneMigrateDirective, say) must not force a protocol version
///          bump on the internal shard mesh, and vice versa.
///
/// Design rules:
///   - Every message is a plain-old-data struct with a leading opcode.
///     The shard mesh never reflects on C++ types across process
///     boundaries; serialization is an explicit responsibility of
///     `ShardMesh::Serialize` (which today knows how to emit each of
///     these structs).
///   - All timestamps use millisecond-granularity monotonic clocks.  We
///     never put wall clocks on the wire because nodes drift and we
///     don't want a migration to hinge on NTP sanity.
///   - Message ids are globally unique inside one source→dest pair but
///     not across the whole mesh.  That keeps the counter cheap (just a
///     per-peer monotonic u64) and the receiver can detect retries by
///     (peer, id) tuple.
///
/// Related headers:
///   - `ConnectionMigration.h` (client-facing directive and token)
///   - `ShardMesh.h`           (delivery fabric)
///   - `ZoneHandoffEntityState` lives in ConnectionMigration.h and is
///     re-used here to avoid duplicating the payload definition.

#pragma once

#include "SagaServer/Networking/Server/ConnectionMigration.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Networking {

// ─── Opcodes ────────────────────────────────────────────────────────────────

/// Wire opcode byte prefixing every shard-mesh message.  One byte is
/// plenty — the shard mesh has maybe a dozen message types total, and
/// keeping the opcode small shaves fixed overhead off every transfer.
enum class ZoneTransferOpcode : std::uint8_t
{
    Invalid        = 0,
    HandoffRequest = 1,
    HandoffAck     = 2,
    HandoffAbort   = 3,  ///< Source cancelling an in-flight request.
    HandoffCommit  = 4,  ///< Client actually reconnected; source may drop it.
    Heartbeat      = 5,  ///< Keeps the mesh connection warm between handoffs.
};

// ─── Common header ──────────────────────────────────────────────────────────

/// Shared prefix on every mesh message.  The receiver reads this first,
/// dispatches by `opcode`, and only then deserialises the tail.  Using a
/// fixed header means a partial read on the mesh socket can be detected
/// before committing buffer space for the rest of the message.
struct ZoneTransferHeader
{
    ZoneTransferOpcode opcode           = ZoneTransferOpcode::Invalid;
    std::uint8_t       protocolVersion  = 1; ///< Bumped when wire layout changes.
    std::uint16_t      payloadSize      = 0; ///< Bytes following this header.
    std::uint64_t      messageId        = 0; ///< Monotonic per source peer.
    std::uint64_t      sourceMonoMs     = 0; ///< Source monotonic clock.
};

// ─── Messages ───────────────────────────────────────────────────────────────

/// Wire counterpart of the client-facing ZoneHandoffRequest.  The client
/// struct lives in ConnectionMigration.h; this one wraps the same payload
/// for mesh transmission and adds routing metadata the client never sees.
struct ZoneTransferRequestMsg
{
    ZoneTransferHeader header{};
    ZoneHandoffRequest request{};   ///< Defined in ConnectionMigration.h.

    /// Authenticated source identity.  The destination checks this
    /// against the shard registry so a compromised zone cannot spoof
    /// handoffs from a zone it does not own.
    std::uint64_t sourceAuthTag = 0;
};

/// Destination's reply.  `ack.rejection == None` means the handoff is
/// accepted and `ack.acceptanceToken` is live.
struct ZoneTransferAckMsg
{
    ZoneTransferHeader header{};
    ZoneHandoffAck     ack{};
};

/// Source aborts a handoff (player logged out before the ack arrived,
/// source zone shutting down, destination took too long).  Destination
/// must free any slot it reserved and release the token.
struct ZoneTransferAbortMsg
{
    ZoneTransferHeader header{};
    std::uint64_t      requestId    = 0;
    std::uint16_t      reasonCode   = 0; ///< Free-form, logged for postmortem.
    std::string        reasonDetail;
};

/// Source has observed the client's reconnect on the destination and is
/// ready to drop its local copy of the entity.  Destination replies with
/// nothing — this is a fire-and-forget cleanup nudge.
struct ZoneTransferCommitMsg
{
    ZoneTransferHeader header{};
    std::uint64_t      requestId = 0;
};

/// Idle heartbeat keeping a mesh link open.  Without these the OS TCP
/// keepalive would still close an idle shard link after its default
/// interval, triggering a spurious reconnect storm.
struct ZoneTransferHeartbeatMsg
{
    ZoneTransferHeader header{};
    std::uint32_t      pendingHandoffs = 0; ///< Purely informational.
};

// ─── Result ─────────────────────────────────────────────────────────────────

/// Return code for mesh-side feed operations.  Distinct from
/// `ZoneHandoffRejection` because the mesh protocol has its own failure
/// modes (malformed header, version mismatch) that have nothing to do
/// with the gameplay-level handoff decision.
enum class ZoneTransferResult : std::uint8_t
{
    Ok = 0,
    ShortRead,             ///< Not enough bytes yet, ask for more.
    InvalidOpcode,
    InvalidHeader,
    ProtocolVersionMismatch,
    UnknownPeer,
    InternalError,
};

// ─── Constants ──────────────────────────────────────────────────────────────

/// Largest shard-mesh message we will accept.  Chosen so that a single
/// handoff fits (ZoneHandoffEntityState with a modest inventory blob is
/// ~2 KiB) but a malicious peer cannot make us allocate megabytes.
inline constexpr std::uint32_t kMaxZoneTransferMessageSize = 64 * 1024;

/// Handoff timeout on the source side.  If the destination hasn't
/// acknowledged within this window we consider the handoff failed and
/// roll the entity back to the source — better a visible stutter than a
/// lost character.
inline constexpr std::uint64_t kZoneHandoffTimeoutMs = 5'000;

/// Acceptance token lifetime on the destination side.  Must exceed the
/// client's reconnect grace period and any realistic zone boundary
/// latency hiccup.  Ten seconds gives mobile clients enough runway to
/// survive a Wi-Fi handoff during a zone crossing.
inline constexpr std::uint64_t kAcceptanceTokenLifetimeMs = 10'000;

} // namespace SagaEngine::Networking
