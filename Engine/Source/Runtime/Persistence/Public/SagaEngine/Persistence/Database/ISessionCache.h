/// @file ISessionCache.h
/// @brief Hot-state session cache backed by Redis (or equivalent).
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: Every client connection carries a session record: who is
///          logged in, which zone owns them, when their auth token
///          expires, which rate-limit bucket they belong to, what
///          their active character is.  Reading those records from
///          PostgreSQL on every incoming packet is absurdly
///          expensive; holding them in process memory breaks down the
///          moment a player reconnects to a different zone server
///          and loses the in-memory state.
///
///          The session cache is the middle ground: a fast key/value
///          store (Redis in production, an in-memory stub in tests)
///          shared across every zone process.  Any process can read,
///          update, or invalidate a session record.  Records have
///          explicit TTLs so abandoned sessions disappear without a
///          sweeping background job.
///
/// Design rules:
///   - The interface is deliberately tiny: Get, Put, Delete, Touch,
///     and a compare-and-set for atomic replace.  Anything fancier
///     (sorted sets, streams) belongs to a different service; the
///     session cache has one job.
///   - Values are opaque byte strings.  Higher layers serialise
///     their own structs (flatbuffer, protobuf, hand-rolled) and
///     this interface never tries to understand them.  That keeps
///     the backend layer free of gameplay dependencies.
///   - TTL is mandatory on Put.  A session without a TTL is a leak
///     waiting to happen; making the caller supply one forces the
///     conversation about how long a session should live.
///   - Every operation is async-friendly — callers that want
///     synchronous behaviour block on the returned future, callers
///     that want fire-and-forget ignore it.  Implementations decide
///     the cost of the abstraction (a coalesced pipeline, a worker
///     thread per shard, etc.).
///
/// What this header is NOT:
///   - Not a general-purpose Redis wrapper.  This is the session
///     subset; pub/sub and rate limiting live in sibling headers.
///   - Not a distributed lock manager.  Sessions are single-writer
///     by construction (the owning zone process), so we do not need
///     full Redlock semantics.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SagaEngine::Persistence::Database {

// ─── Result codes ──────────────────────────────────────────────────────────

/// Outcome of a session-cache operation.  Distinct codes so the
/// caller can branch on "not found" vs "backend is down" — those
/// have different recovery strategies.
enum class SessionCacheStatus : std::uint8_t
{
    Pending = 0,
    Ok,
    NotFound,
    VersionMismatch,    ///< Compare-and-set failed; caller should re-read.
    BackendError,
    Timeout,
    ShuttingDown,
    InternalError,
};

// ─── Record payload ────────────────────────────────────────────────────────

/// One session record as seen by callers.  `version` drives the
/// compare-and-set path so concurrent updaters converge without
/// overwriting each other.  `ttlSeconds` is the REMAINING time the
/// record has when it was read, not the absolute expiry time.
struct SessionRecord
{
    std::string               key;
    std::vector<std::uint8_t> value;
    std::uint64_t             version    = 0;
    std::uint32_t             ttlSeconds = 0;
};

// ─── Async handle ──────────────────────────────────────────────────────────

/// Future-like result for a single operation.  Shared ownership lets
/// callers pass handles across subsystems; the implementation
/// publishes the final state once and the observers see a consistent
/// snapshot.
class SessionCacheOperation
{
public:
    SessionCacheOperation() = default;

    [[nodiscard]] bool IsReady() const noexcept
    {
        return status_.load(std::memory_order_acquire) != SessionCacheStatus::Pending;
    }

    [[nodiscard]] SessionCacheStatus Status() const noexcept
    {
        return status_.load(std::memory_order_acquire);
    }

    [[nodiscard]] const SessionRecord& Record() const noexcept { return record_; }

    // ── Internal ────────────────────────────────────────────────────────
    void SetRecord(SessionRecord&& record) noexcept
    {
        record_ = std::move(record);
        status_.store(SessionCacheStatus::Ok, std::memory_order_release);
    }
    void SetStatus(SessionCacheStatus status) noexcept
    {
        status_.store(status, std::memory_order_release);
    }

private:
    std::atomic<SessionCacheStatus> status_{SessionCacheStatus::Pending};
    SessionRecord                   record_;
};

using SessionCacheOperationPtr = std::shared_ptr<SessionCacheOperation>;

// ─── Interface ─────────────────────────────────────────────────────────────

/// Abstract session cache.  One instance per deployment; every zone
/// process in a cluster talks to the same logical cache so
/// reconnecting players find their state wherever they land.
class ISessionCache
{
public:
    virtual ~ISessionCache() = default;

    /// Connect to the backing store.  `endpoint` is opaque to the
    /// interface; Redis implementations expect "redis://host:port".
    /// Returns false on the initial connect failure — callers
    /// typically escalate, because running without a session cache
    /// is not a supported mode.
    [[nodiscard]] virtual bool Start(const std::string& endpoint) = 0;

    /// Shut down and release every connection.  In-flight operations
    /// settle to `ShuttingDown`.
    virtual void Stop() = 0;

    /// Read a session by key.  Populates the handle's record on
    /// success; leaves it untouched on NotFound.
    [[nodiscard]] virtual SessionCacheOperationPtr Get(
        std::string_view key) = 0;

    /// Unconditional write.  Overwrites any existing value and bumps
    /// the version counter.  `ttlSeconds` MUST be non-zero — the
    /// interface rejects zero at the type level via a defensive
    /// check in every implementation.
    [[nodiscard]] virtual SessionCacheOperationPtr Put(
        std::string_view                 key,
        const std::vector<std::uint8_t>& value,
        std::uint32_t                    ttlSeconds) = 0;

    /// Compare-and-set.  Write succeeds only if the server-side
    /// record has `expectedVersion`.  Returns `VersionMismatch` if
    /// another writer raced ahead; the caller re-reads and retries.
    [[nodiscard]] virtual SessionCacheOperationPtr CompareAndSet(
        std::string_view                 key,
        std::uint64_t                    expectedVersion,
        const std::vector<std::uint8_t>& newValue,
        std::uint32_t                    ttlSeconds) = 0;

    /// Extend the TTL of an existing record without rewriting the
    /// value.  Used to keep an active session warm while the player
    /// is still connected.
    [[nodiscard]] virtual SessionCacheOperationPtr Touch(
        std::string_view key,
        std::uint32_t    ttlSeconds) = 0;

    /// Remove a session.  Idempotent — deleting a non-existent key
    /// still returns Ok, because "the session is gone" is what the
    /// caller wanted regardless of whether it was there a moment
    /// ago.
    [[nodiscard]] virtual SessionCacheOperationPtr Delete(
        std::string_view key) = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using SessionCachePtr = std::shared_ptr<ISessionCache>;

// ─── Constants ─────────────────────────────────────────────────────────────

/// Default TTL for a fresh login session.  Chosen to outlast any
/// reasonable zone crossing plus a grace period for mobile network
/// hiccups.  Services are free to pass a longer or shorter value.
inline constexpr std::uint32_t kDefaultSessionTtlSeconds = 600;

/// Hard ceiling on a single session value.  Sessions are metadata,
/// not content — anything larger than this is almost certainly a
/// misuse of the cache (store it in PostgreSQL or in a blob store).
inline constexpr std::uint32_t kMaxSessionValueBytes = 16 * 1024;

} // namespace SagaEngine::Persistence::Database
