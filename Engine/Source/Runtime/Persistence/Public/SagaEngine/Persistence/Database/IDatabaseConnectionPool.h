/// @file IDatabaseConnectionPool.h
/// @brief Abstract connection pool for SQL backends.
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: Opening a fresh PostgreSQL connection costs tens of
///          milliseconds — far too long to pay on every query the
///          gameplay services fire off.  A pool keeps a bounded set of
///          live connections warm, hands them out on demand, and
///          recycles them back into the pool when the caller is done.
///          This header defines the *contract* every pool
///          implementation must satisfy so higher layers (async query
///          executor, migration runner, presence tracker) can depend
///          on the interface without caring whether they are talking
///          to libpq, ODBC, or an in-memory test stub.
///
/// Design rules:
///   - Acquire is non-blocking by default.  The caller gets either a
///     handle immediately or a "pool exhausted" error; the caller
///     decides whether to retry, enqueue, or drop the work.  Blocking
///     inside a connection-acquire call would pin the simulation
///     thread the moment the database gets slow.
///   - Handles are RAII.  A `ConnectionHandle` returns itself to the
///     pool on destruction, so there is no separate "release" call
///     to forget.  The pool still exposes an explicit `Release` for
///     callers that need deterministic lifetime management.
///   - The pool is thread-safe.  Any thread may `Acquire`; any thread
///     may release.  Implementations are free to use a mutex, a
///     lock-free stack, or whatever fits their backend best.
///   - Connections that throw an error are marked unhealthy and
///     recycled.  A bad connection never re-enters the pool — the
///     next caller would hit the same error.
///
/// What this header is NOT:
///   - Not a query interface.  The caller uses whatever statement API
///     the backend exposes via the raw connection pointer.  The pool
///     manages lifetime and health; it does not reimplement libpq.
///   - Not a transaction manager.  Transactions live on top of the
///     connection; the pool has no opinion about them.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace SagaEngine::Persistence::Database {

// ─── Configuration ──────────────────────────────────────────────────────────

/// Per-pool tuning values.  Picked for PostgreSQL defaults but
/// equally applicable to any other SQL backend.
struct DatabasePoolConfig
{
    /// Backend connection string.  Opaque to the pool; passed through
    /// to the implementation when it opens a fresh connection.
    std::string connectionString;

    /// Minimum number of connections kept warm even when idle.  Too
    /// low and the first query after a quiet period pays the full
    /// connect cost; too high and the database wastes process slots
    /// on connections nobody needs.
    std::uint32_t minConnections = 4;

    /// Hard cap on simultaneous connections.  Protects the database
    /// from a thundering herd and mirrors the operator's per-backend
    /// configuration (PostgreSQL's `max_connections`, etc.).
    std::uint32_t maxConnections = 32;

    /// How long a connection may sit idle before the pool closes it.
    /// Longer values reduce churn; shorter values free up database
    /// slots faster during quiet periods.
    std::uint32_t idleTimeoutSeconds = 300;

    /// Maximum lifetime of any one connection, regardless of idle
    /// state.  Exists so operators can recycle connections through a
    /// rolling certificate change without bouncing the whole pool.
    std::uint32_t maxLifetimeSeconds = 3600;

    /// Statement run before a connection is handed out for the first
    /// time.  Use it to `SET application_name`, adjust search paths,
    /// or pin the timezone.  Empty = skip.
    std::string initialStatement;
};

// ─── Opaque connection handle ──────────────────────────────────────────────

/// Forward-declared raw connection type.  Each backend defines its own
/// concrete handle (`pq_conn*`, etc.) and the pool implementation
/// casts through this opaque pointer.  Keeping it `void*` here means
/// the interface does not leak libpq headers into the call sites.
using RawConnection = void*;

/// Acquire outcome.  Distinct codes so the caller can branch on
/// "retry is worthwhile" vs "fail loud".
enum class AcquireResult : std::uint8_t
{
    Ok = 0,
    PoolExhausted,        ///< All connections in use — try again later.
    BackendUnavailable,   ///< Unable to open a new connection to the database.
    ShuttingDown,         ///< Pool is tearing down; reject new work.
    InternalError,
};

/// RAII wrapper returned by `Acquire`.  Destructor calls `Release`
/// on the owning pool, so a stack-scoped handle is the safe default.
/// Move-only — copying would double-release on the second destructor.
class ConnectionHandle
{
public:
    ConnectionHandle() noexcept = default;
    ConnectionHandle(class IDatabaseConnectionPool* owner,
                     RawConnection                  raw) noexcept
        : owner_(owner), raw_(raw)
    {
    }

    ConnectionHandle(const ConnectionHandle&)            = delete;
    ConnectionHandle& operator=(const ConnectionHandle&) = delete;

    ConnectionHandle(ConnectionHandle&& other) noexcept
        : owner_(other.owner_), raw_(other.raw_)
    {
        other.owner_ = nullptr;
        other.raw_   = nullptr;
    }

    ConnectionHandle& operator=(ConnectionHandle&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            owner_       = other.owner_;
            raw_         = other.raw_;
            other.owner_ = nullptr;
            other.raw_   = nullptr;
        }
        return *this;
    }

    ~ConnectionHandle() { Release(); }

    /// True when the handle owns a live connection.  A moved-from
    /// handle returns false; so does a default-constructed one.
    [[nodiscard]] bool IsValid() const noexcept { return raw_ != nullptr; }

    /// Access the backend-specific raw pointer.  The caller casts to
    /// whatever concrete type matches the backend.  Returns `nullptr`
    /// on an invalid handle so callers can check at the call site.
    [[nodiscard]] RawConnection Raw() const noexcept { return raw_; }

    /// Mark the underlying connection as unhealthy.  The pool will
    /// close it on release instead of returning it to the idle list.
    /// Call this after any backend error that might have corrupted
    /// transaction state.
    void MarkUnhealthy() noexcept { unhealthy_ = true; }

    /// Explicit release.  Rarely needed — the destructor covers most
    /// call sites — but useful when the caller wants to free the
    /// connection before a long non-database operation.
    void Release() noexcept;

private:
    class IDatabaseConnectionPool* owner_    = nullptr;
    RawConnection                  raw_      = nullptr;
    bool                           unhealthy_= false;
};

// ─── Statistics snapshot ────────────────────────────────────────────────────

/// Diagnostic counters returned by `Snapshot()`.  Cheap to read;
/// intended for `/dbstat` style operator commands and for emitting
/// metrics to the telemetry pipeline.
struct DatabasePoolStats
{
    std::uint32_t totalConnections  = 0;
    std::uint32_t idleConnections   = 0;
    std::uint32_t busyConnections   = 0;
    std::uint64_t totalAcquires     = 0;
    std::uint64_t failedAcquires    = 0;
    std::uint64_t unhealthyRecycles = 0;
};

// ─── Interface ──────────────────────────────────────────────────────────────

/// Abstract connection pool.  One instance per backend, shared across
/// every service that talks to that backend.
class IDatabaseConnectionPool
{
public:
    virtual ~IDatabaseConnectionPool() = default;

    /// Start the pool and warm up `minConnections` connections.
    /// Returns `false` if the backend rejected the initial open — in
    /// that case the caller can retry with backoff or escalate to the
    /// operator.  Must be called exactly once before any `Acquire`.
    [[nodiscard]] virtual bool Start(const DatabasePoolConfig& config) = 0;

    /// Drain the pool, close every connection, and refuse future
    /// acquires.  In-flight handles keep working until the caller
    /// releases them, at which point they close instead of recycling.
    virtual void Stop() = 0;

    /// Try to acquire a connection.  Populates `outHandle` on success
    /// and returns `AcquireResult::Ok`.  On any other result
    /// `outHandle` is left empty.  Non-blocking — the caller decides
    /// retry policy.
    [[nodiscard]] virtual AcquireResult Acquire(ConnectionHandle& outHandle) = 0;

    /// Internal entry point used by `ConnectionHandle` to return a
    /// connection to the pool.  Not part of the public contract —
    /// callers should let RAII drive this.
    virtual void Release(RawConnection raw, bool unhealthy) noexcept = 0;

    /// Human-readable backend name for logs (`"postgresql"`,
    /// `"sqlite-inmemory"`, etc.).
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;

    /// Snapshot of pool-wide counters.  Thread-safe; implementations
    /// typically take a short lock to read the counters atomically.
    [[nodiscard]] virtual DatabasePoolStats Snapshot() const = 0;
};

using DatabasePoolPtr = std::shared_ptr<IDatabaseConnectionPool>;

// ─── Inline definitions ─────────────────────────────────────────────────────

inline void ConnectionHandle::Release() noexcept
{
    // Nothing to do on a moved-from or default-constructed handle.
    // Checking here means the destructor does not have to duplicate
    // the guard, and callers can safely re-release.
    if (owner_ != nullptr && raw_ != nullptr)
    {
        owner_->Release(raw_, unhealthy_);
    }
    owner_     = nullptr;
    raw_       = nullptr;
    unhealthy_ = false;
}

} // namespace SagaEngine::Persistence::Database
