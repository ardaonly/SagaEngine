/// @file IPresenceTracker.h
/// @brief Cluster-wide online / offline / zone-resident presence tracking.
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: Clients, operators, and gameplay systems all want to know
///          the same handful of facts about an account: is it online,
///          which zone does it currently live on, when did it last
///          check in, which shard process owns the connection.  That
///          information is highly mutable — it flips every time a
///          player crosses a zone boundary — so it does not belong in
///          PostgreSQL.  Instead the session cache holds the raw
///          bytes and this tracker exposes the presence-shaped view
///          on top.
///
/// Design rules:
///   - The tracker is a *view*, not a data store.  Updates translate
///     into session-cache writes; reads translate into session-cache
///     reads.  The tracker caches nothing locally because any cache
///     would get stale the instant another process published a
///     change.
///   - Heartbeats are the single source of truth for "is this player
///     still here".  The owning zone server sends a heartbeat every
///     `kPresenceHeartbeatIntervalSeconds`; any record whose
///     `lastHeartbeatMonoMs` is older than
///     `kPresenceHeartbeatTimeoutSeconds` is considered offline.  No
///     background sweeper is required — staleness is inferred on
///     read.
///   - Lookups take an account id, not a connection id.  Connections
///     come and go; the account is the stable identity every
///     gameplay system cares about.
///   - The tracker refuses to publish a record for an account that
///     is already online elsewhere unless the caller opts into a
///     forced takeover.  That guards against a classic double-login
///     race where a reconnect arrives before the old session has
///     noticed its own disconnect.
///
/// What this header is NOT:
///   - Not a friends list.  Social-graph queries live in a separate
///     service that may or may not consult this tracker.
///   - Not a chat backend.  Presence answers "who is online"; chat
///     rides the pub/sub bus.

#pragma once

#include "Services/Persistence/Database/ISessionCache.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Persistence::Database {

// ─── Identifiers ───────────────────────────────────────────────────────────

/// Account id as used by the rest of the engine.  Re-declared here
/// instead of pulling in the server-side definition, because the
/// backend layer must not depend on `SagaServer`.
using PresenceAccountId = std::uint64_t;

/// Zone identifier used by the tracker.  Matches the `ZoneId` used
/// by the network layer; re-declared for the same reason as above.
using PresenceZoneId = std::uint32_t;

// ─── Presence record ───────────────────────────────────────────────────────

/// Presence state as observed by the tracker.  `Online` means there
/// is a fresh heartbeat inside the timeout window; `Stale` means the
/// record exists but the heartbeat is too old to trust (probably a
/// crashed zone process); `Offline` means no record is present at
/// all.
enum class PresenceState : std::uint8_t
{
    Offline = 0,
    Online  = 1,
    Stale   = 2,
};

/// Record the tracker publishes into the session cache and
/// reconstructs on read.  `schemaVersion` exists so a cluster
/// running a mix of binary versions during a rolling upgrade can
/// detect an incompatible record and fall back to treating it as
/// stale.
struct PresenceRecord
{
    std::uint16_t       schemaVersion       = 1;
    PresenceAccountId   account             = 0;
    PresenceZoneId      currentZone         = 0;
    std::uint64_t       ownerShardId        = 0;   ///< Shard process that owns the connection.
    std::uint64_t       loginMonoMs         = 0;   ///< When the player first came online this session.
    std::uint64_t       lastHeartbeatMonoMs = 0;   ///< Clock the freshness check uses.
    std::string         characterName;              ///< Optional display hint for operators.
    std::vector<std::uint8_t> extraBlob;            ///< Free-form expansion slot.
};

// ─── Operation status ──────────────────────────────────────────────────────

/// Result of a presence operation.  Distinct from `SessionCacheStatus`
/// because a successful cache write can still represent a logical
/// failure (another process already owns the account).
enum class PresenceResult : std::uint8_t
{
    Pending = 0,
    Ok,
    NotOnline,          ///< Lookup: account is Offline.
    Stale,              ///< Lookup: record exists but heartbeat is too old.
    AlreadyOnline,      ///< Publish: another shard owns the account, takeover not requested.
    CacheError,         ///< Underlying session cache returned BackendError.
    InternalError,
};

// ─── Async handle ──────────────────────────────────────────────────────────

/// Future-like result for a tracker operation.  Mirrors the shape of
/// `SessionCacheOperation` so call sites that deal with both feel
/// consistent.
class PresenceOperation
{
public:
    PresenceOperation() = default;

    [[nodiscard]] bool IsReady() const noexcept
    {
        return status_.load(std::memory_order_acquire) != PresenceResult::Pending;
    }

    [[nodiscard]] PresenceResult Status() const noexcept
    {
        return status_.load(std::memory_order_acquire);
    }

    [[nodiscard]] const PresenceRecord& Record() const noexcept { return record_; }
    [[nodiscard]] PresenceState         State() const noexcept { return state_; }

    // ── Internal ────────────────────────────────────────────────────────
    void SetRecord(PresenceRecord&& record, PresenceState state) noexcept
    {
        record_ = std::move(record);
        state_  = state;
        status_.store(PresenceResult::Ok, std::memory_order_release);
    }
    void SetStatus(PresenceResult status) noexcept
    {
        status_.store(status, std::memory_order_release);
    }

private:
    std::atomic<PresenceResult> status_{PresenceResult::Pending};
    PresenceRecord              record_{};
    PresenceState               state_ = PresenceState::Offline;
};

using PresenceOperationPtr = std::shared_ptr<PresenceOperation>;

// ─── Interface ─────────────────────────────────────────────────────────────

/// Abstract tracker.  One instance per process, shared by every
/// subsystem that wants to read or publish presence.  The tracker
/// does not own its session cache — multiple trackers can share one
/// cache if, for example, a stress test wants to spin up several.
class IPresenceTracker
{
public:
    virtual ~IPresenceTracker() = default;

    /// Bind to a started session cache.  The tracker does not
    /// invoke `ISessionCache::Start`; the caller must have done so.
    [[nodiscard]] virtual bool Start(SessionCachePtr sessionCache) = 0;
    virtual void Stop() = 0;

    /// Publish that `account` is now online on `zone`.  Fails with
    /// `AlreadyOnline` if another record exists and `forceTakeover`
    /// is false.
    [[nodiscard]] virtual PresenceOperationPtr Publish(
        const PresenceRecord& record,
        bool                  forceTakeover) = 0;

    /// Heartbeat the owning shard's liveness without rewriting the
    /// rest of the record.  Cheap — bumps the TTL in the underlying
    /// session cache.
    [[nodiscard]] virtual PresenceOperationPtr Heartbeat(
        PresenceAccountId account) = 0;

    /// Remove the record.  Used on clean logout.  A crashed shard
    /// never calls this; the staleness check on read handles that
    /// case.
    [[nodiscard]] virtual PresenceOperationPtr Clear(
        PresenceAccountId account) = 0;

    /// Look up a single account.  The handle reports `Ok` with a
    /// populated record on success, `NotOnline` if no record exists,
    /// or `Stale` if a record exists but its heartbeat is expired.
    [[nodiscard]] virtual PresenceOperationPtr Lookup(
        PresenceAccountId account) const = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using PresenceTrackerPtr = std::shared_ptr<IPresenceTracker>;

// ─── Constants ─────────────────────────────────────────────────────────────

/// Recommended heartbeat cadence.  Picked so a single missed
/// heartbeat does NOT flip a record to Stale — the timeout below
/// covers at least three intervals of slack.
inline constexpr std::uint32_t kPresenceHeartbeatIntervalSeconds = 10;

/// Staleness threshold.  Any record whose heartbeat is older than
/// this is treated as Stale on read.  Generous enough to absorb a
/// brief GC pause on the owning shard without triggering a ghost
/// logout.
inline constexpr std::uint32_t kPresenceHeartbeatTimeoutSeconds = 45;

/// Session-cache TTL used for presence records.  Slightly longer
/// than the timeout so a record that legitimately misses a
/// heartbeat still has a narrow window to be refreshed before the
/// cache evicts it outright.
inline constexpr std::uint32_t kPresenceRecordTtlSeconds = 60;

} // namespace SagaEngine::Persistence::Database
