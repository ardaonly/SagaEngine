/// @file DynamicZoneSpawner.h
/// @brief On-demand zone process creation and teardown protocol.
///
/// Layer  : SagaServer / Networking / Server
/// Purpose: Static zone allocation (one process per named zone, started
///          at cluster boot) is fine for a launch-day map, but it
///          wastes compute as soon as the world has optional content:
///          instanced dungeons, player-built housing plots, seasonal
///          events, or overflow shards when a zone hits capacity.
///
///          This header defines the contract the orchestrator uses to
///          ask the shard manager "please spin up a zone with this
///          template at this location", and the contract the zone
///          process uses to announce "I'm ready, peers may connect to
///          me at this endpoint".
///
///          The spawner itself is implementation-specific (one backend
///          uses Kubernetes jobs, another uses local fork+exec for
///          single-host dev boxes).  This header only fixes the
///          *protocol* so those implementations are interchangeable.
///
/// Lifecycle:
///   1. Orchestrator decides a new zone is needed (capacity trigger,
///      instanced dungeon request, GM command).
///   2. Orchestrator calls `IDynamicZoneSpawner::Spawn` with a template
///      id and instance parameters.  Spawn returns a pending handle.
///   3. The spawned process loads its template, binds its endpoint, and
///      POSTs a `ZoneReadyNotice` back to the spawner.
///   4. Spawner resolves the pending handle, registers the new peer
///      with the shard mesh, and notifies subscribers via the
///      `ZoneLifecycleCallback`.
///   5. On teardown the spawner sends a `DrainRequest`; the zone
///      refuses new connections, migrates remaining players, and
///      exits.
///
/// What this header is NOT:
///   - Not an OS-level process manager.  The spawner may be backed by
///     a container orchestrator, a custom init script, or a local test
///     harness — this header never reaches through to fork/exec.

#pragma once

#include "SagaServer/Networking/Server/ConnectionMigration.h" // ZoneId, AccountId

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Networking {

// ─── Template identifiers ───────────────────────────────────────────────────

/// Stable handle for a zone *template*.  Templates describe the static
/// data needed to spin up an instance — map asset, spawn rules, gameplay
/// scripts, capacity caps.  Templates are compiled into the content
/// pipeline and therefore come with a stable id across releases.
using ZoneTemplateId = std::uint32_t;

/// Transient handle returned by `Spawn()` before the child process has
/// reported ready.  Becomes a real `ZoneId` once the child reports in.
using PendingZoneSpawnId = std::uint64_t;

// ─── Spawn parameters ───────────────────────────────────────────────────────

/// Per-spawn arguments.  The template decides everything static; these
/// are the values that vary instance-to-instance.
struct ZoneSpawnParameters
{
    ZoneTemplateId templateId = 0;

    /// Optional seed for procedural content.  Zero means "roll a fresh
    /// seed on the child side"; any other value is passed through so
    /// replays of a procedurally-generated dungeon stay identical.
    std::uint64_t contentSeed = 0;

    /// Instance-owner metadata.  Private instances (dungeons) record
    /// the party leader's account here so the spawner can return the
    /// *existing* instance on a duplicate request.
    AccountId ownerAccount = 0;

    /// Maximum players allowed on this instance.  May be lower than
    /// the template default (scaled down for small parties) but may
    /// not exceed it.  Enforced server-side, not trusted from clients.
    std::uint32_t playerCapHint = 0;

    /// Free-form tag string written to logs and /meshstat.  Useful for
    /// tying a spawn back to the event that caused it ("invasion
    /// 2026-04", "raid-crystalfall-10man").
    std::string diagnosticsTag;
};

// ─── Outcome codes ──────────────────────────────────────────────────────────

/// Reasons a spawn request may fail.  Each one maps to a distinct
/// recovery on the orchestrator side.
enum class ZoneSpawnError : std::uint8_t
{
    None = 0,
    UnknownTemplate,        ///< templateId is not registered.
    QuotaExceeded,          ///< Cluster has no free slots for this template.
    BackendUnreachable,     ///< Container orchestrator is down.
    InvalidParameters,      ///< playerCapHint exceeds template cap, etc.
    InternalError,
};

/// Result of a spawn request.  On success, `pendingId` identifies the
/// still-booting zone and later becomes the stable ZoneId via the
/// ready notice.
struct ZoneSpawnResult
{
    ZoneSpawnError     error     = ZoneSpawnError::None;
    PendingZoneSpawnId pendingId = 0;
    std::string        reason;  ///< Operator-facing detail when error != None.
};

// ─── Ready notice (child → spawner) ─────────────────────────────────────────

/// The child process POSTs this back to the spawner once it has loaded
/// the template, bound its listening socket, and is ready to accept
/// peer connections.  The spawner uses this to complete the pending
/// spawn and promote it into a live ZoneId.
struct ZoneReadyNotice
{
    PendingZoneSpawnId pendingId    = 0;
    ZoneId             assignedZoneId = 0;
    std::string        endpoint;           ///< Where the mesh should dial.
    std::uint64_t      readyMonoMs  = 0;   ///< Child monotonic clock at ready.
    std::uint32_t      initialCapacity = 0;
};

// ─── Drain request (spawner → child) ────────────────────────────────────────

/// Spawner asks the child to empty out gracefully.  The child refuses
/// new connections, walks any remaining players through zone migration,
/// and then exits.  Force-kill only happens if the child misses the
/// grace window — tracked separately by the orchestrator.
struct ZoneDrainRequest
{
    ZoneId        zone         = 0;
    std::uint32_t graceSeconds = 60;

    /// Reason string.  Logged on the child's shutdown path so operators
    /// can tell a "routine cost-saving drain" from an "emergency drain
    /// because the host is being patched".
    std::string reason;
};

// ─── Lifecycle callback ─────────────────────────────────────────────────────

/// Subscribers use this to react to zone lifecycle events without
/// polling the spawner.  The orchestrator, shard mesh, and /meshstat
/// command are all subscribers.
enum class ZoneLifecycleEvent : std::uint8_t
{
    SpawnRequested = 0,
    SpawnReady     = 1,
    SpawnFailed    = 2,
    DrainStarted   = 3,
    Stopped        = 4,
};

using ZoneLifecycleCallback = std::function<void(
    ZoneLifecycleEvent event,
    ZoneId             zone,
    PendingZoneSpawnId pendingId)>;

// ─── Interface ──────────────────────────────────────────────────────────────

/// Abstract spawner.  One instance per cluster, usually embedded in
/// the orchestrator service rather than in a gameplay zone.
class IDynamicZoneSpawner
{
public:
    virtual ~IDynamicZoneSpawner() = default;

    /// Request a new zone.  Returns immediately with either a pending
    /// handle or an error; completion arrives via the lifecycle
    /// callback.
    [[nodiscard]] virtual ZoneSpawnResult Spawn(
        const ZoneSpawnParameters& params) = 0;

    /// Request a graceful drain.  Safe to call on an already-draining
    /// zone — the second call becomes a no-op.  Returns false only if
    /// `zone` is not known to this spawner.
    virtual bool RequestDrain(const ZoneDrainRequest& request) = 0;

    /// Invoked by the backend when a child reports ready.  Implementations
    /// route this to the pending spawn record and fire the callback.
    virtual void OnChildReady(const ZoneReadyNotice& notice) = 0;

    /// Install the lifecycle subscriber.  Single-subscriber by design;
    /// compose multiple listeners outside this interface if needed.
    virtual void SetLifecycleCallback(ZoneLifecycleCallback cb) = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using DynamicZoneSpawnerPtr = std::shared_ptr<IDynamicZoneSpawner>;

// ─── Constants ──────────────────────────────────────────────────────────────

/// Upper bound on how long a pending spawn may stay pending before the
/// orchestrator considers it failed.  Container orchestrators can take
/// 30-60 seconds cold; 120 covers that plus a content-loading margin.
inline constexpr std::uint32_t kZoneSpawnTimeoutSeconds = 120;

/// Hard cap on active pending spawns.  Protects the orchestrator from a
/// feedback loop where capacity triggers spawn more instances, the new
/// instances trip the same trigger, and so on.
inline constexpr std::uint32_t kMaxPendingSpawns = 256;

} // namespace SagaEngine::Networking
