/// @file CrossZoneVisibility.h
/// @brief Shared visibility of boundary entities between adjacent zones.
///
/// Layer  : SagaServer / Networking / Server
/// Purpose: Zones are not isolated islands.  A player standing near a
///          zone boundary can *see* players, NPCs, and effects from the
///          neighbouring zone long before they step across.  If we only
///          replicated the zone each client currently owns, the world
///          would visibly pop: walk towards a bridge and half the scene
///          would materialise as soon as you crossed.
///
///          This header defines the protocol by which adjacent zones
///          *share* entity state in a narrow "apron" around their shared
///          boundary.  Entities in the apron are authoritative on their
///          home zone but read-only replicas on the neighbour; the
///          neighbour forwards visibility updates to its own clients so
///          those clients see the entity without a full handoff.
///
/// Design rules:
///   - The apron width is a *configuration* value per zone boundary,
///     not a hardcoded constant.  Some boundaries are indoor/outdoor
///     transitions with a tiny apron; others are open-world borders
///     with a 200-metre apron.  The scripting layer owns this choice.
///   - Replicas are strictly read-only.  Neighbour zones never write to
///     a replicated entity.  Any incoming command from a client targeted
///     at a replica is rejected — the client must be told to talk to
///     the authoritative zone.
///   - Updates are delta-encoded against the last snapshot the neighbour
///     acknowledged.  Full snapshots only ride on entry or after an
///     explicit resync request.
///
/// This file is pure data definitions: the protocol structs, the
/// enum codes for replica entry/exit events, and the config type the
/// shard mesh reads.  The scheduler and the actual send/receive loop
/// live in `ShardMesh.h` and its implementation.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaServer/Networking/Server/ConnectionMigration.h" // for ZoneId

#include <cstdint>
#include <vector>

namespace SagaEngine::Networking {

// ─── Entity identification ──────────────────────────────────────────────────

/// Stable cross-zone entity handle.  The zone id disambiguates handles
/// that happen to collide across processes, the local id is whatever
/// the home zone's ECS uses (usually the entity index).  The pair is
/// unique for the lifetime of that entity in the mesh.
struct CrossZoneEntityId
{
    ZoneId        homeZone = 0;
    std::uint64_t localId  = 0;

    [[nodiscard]] constexpr bool operator==(const CrossZoneEntityId& o) const noexcept
    {
        return homeZone == o.homeZone && localId == o.localId;
    }
};

// ─── Apron configuration ────────────────────────────────────────────────────

/// Per-boundary apron descriptor.  One instance lives on each side of a
/// boundary; the two sides do NOT have to agree on width (asymmetric
/// sight lines are legitimate — an indoor zone may want to see outdoor
/// NPCs from far away but not vice versa).
struct CrossZoneApronConfig
{
    ZoneId neighbourZone = 0;

    /// Apron extent in metres.  Entities within this distance of the
    /// shared boundary are sent to `neighbourZone` as replicas.
    float apronWidth = 0.0f;

    /// Maximum number of replicas this zone is willing to publish to
    /// the neighbour.  A hard cap protects against pathological open-
    /// world scenarios (a stampede of NPCs piling against a boundary).
    /// Excess replicas are dropped in priority order.
    std::uint32_t maxReplicas = 512;

    /// Tick cadence for apron updates.  Faster = more bandwidth, finer
    /// visual smoothness on the other side.  Typically half the home
    /// zone's replication rate — apron updates do not need to match
    /// authoritative updates one-for-one.
    std::uint32_t updateHz = 10;
};

// ─── Replica lifecycle events ───────────────────────────────────────────────

/// Reason an entity entered or left a neighbour's apron.  Used by logs
/// and by the client UI layer (a disappearing NPC needs the right fade-
/// out behaviour depending on *why* it disappeared).
enum class CrossZoneReplicaEvent : std::uint8_t
{
    Enter      = 0, ///< Entity stepped into the apron for the first time.
    Leave      = 1, ///< Entity stepped out — stop replicating.
    Destroyed  = 2, ///< Entity died / despawned on its home zone.
    HomeMoved  = 3, ///< Entity transferred home zones; replica promoted elsewhere.
};

/// Wire format for an entry/exit notification.  The destination uses
/// these to allocate or free its replica record.
struct CrossZoneReplicaNotice
{
    CrossZoneEntityId     entity{};
    CrossZoneReplicaEvent event = CrossZoneReplicaEvent::Enter;
    std::uint32_t         replicationPriority = 0; ///< 0 = highest, N = lowest.
};

// ─── Replica update payload ─────────────────────────────────────────────────

/// Per-tick delta for a single replica.  The field list is deliberately
/// tight: position, rotation, velocity, and a generic "visual state"
/// bitmask (stance, weapon drawn, casting) drive 99% of the visible
/// behaviour for a read-only replica.  Anything fancier (buff icons,
/// nameplate text) can ride in the opaque `extraBlob`.
struct CrossZoneReplicaUpdate
{
    CrossZoneEntityId         entity{};
    SagaEngine::Math::Vec3    position{};
    /// Rotation is stored quantised as a 32-bit packed quaternion for
    /// apron bandwidth reasons — rendering does not care about the
    /// precision lost in the quantisation, and the saved bytes add up
    /// across hundreds of replicas per frame.
    std::uint32_t             packedRotation = 0;
    SagaEngine::Math::Vec3    velocity{};
    std::uint32_t             visualStateBits = 0;
    std::vector<std::uint8_t> extraBlob;
};

// ─── Full apron frame ───────────────────────────────────────────────────────

/// One tick's worth of apron state from source → neighbour.  Sent at
/// the cadence in `CrossZoneApronConfig::updateHz`.  Events always
/// precede updates so the receiver can allocate records before filling
/// them.
struct CrossZoneApronFrame
{
    ZoneId                              sourceZone      = 0;
    ZoneId                              destinationZone = 0;
    std::uint64_t                       sourceMonoMs    = 0;
    std::uint64_t                       frameSequence   = 0; ///< Monotonic per link.
    std::vector<CrossZoneReplicaNotice> events;
    std::vector<CrossZoneReplicaUpdate> updates;
};

// ─── Limits ─────────────────────────────────────────────────────────────────

/// Safety cap on apron frame size.  An apron frame that exceeds this is
/// dropped with a warning — it almost certainly indicates a scheduling
/// bug (two frames coalesced) or a hostile neighbour trying to exhaust
/// our memory.
inline constexpr std::uint32_t kMaxApronFrameBytes = 256 * 1024;

/// Hard ceiling on updates per frame regardless of configuration.  A
/// single apron frame with more than this many entities indicates the
/// configured width is wrong for the zone density.
inline constexpr std::uint32_t kMaxApronUpdatesPerFrame = 4096;

} // namespace SagaEngine::Networking
