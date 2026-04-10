/// @file IAsyncQueryExecutor.h
/// @brief Non-blocking SQL query execution fronted by a pool.
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: The gameplay services must not block on database I/O.  A
///          synchronous `SELECT` running inside the simulation thread
///          stalls every zone tick while the query executor waits on
///          the socket; on a busy server the stall compounds into a
///          visible tick-rate drop.  This header defines an
///          asynchronous query executor: the caller submits a query,
///          receives a future-like handle, and polls or subscribes
///          for the result on a thread that is allowed to block.
///
/// Design rules:
///   - The executor owns a private worker pool.  Submissions are
///     placed on an internal queue; worker threads pull from the
///     queue, acquire a connection from the `IDatabaseConnectionPool`,
///     run the query, and publish the result back via the handle.
///     The caller never touches libpq directly.
///   - Results are copied into the handle before the worker releases
///     the connection.  That way a slow consumer cannot pin a pool
///     connection indefinitely.  The trade-off is the copy cost; for
///     gameplay-sized result sets (hundreds of rows, not millions)
///     the copy is cheaper than the alternative.
///   - Cancellation is cooperative.  `Cancel` flips a flag on the
///     handle; workers that have not yet started the query honour it,
///     workers that have already begun finish the query and discard
///     the result.  Forcibly killing an in-flight query against
///     PostgreSQL is a mess we are not going to get into.
///   - Failures are data, not exceptions.  The handle carries a
///     status code and an error message; callers branch on status in
///     the same control flow that handles success.  The hot loop
///     stays exception-free.
///
/// What this header is NOT:
///   - Not an ORM.  Columns come back as strings and the caller
///     parses them.  The intent is a clean boundary, not a fluent
///     DSL.
///   - Not a transaction manager.  Transactions require a stable
///     connection across multiple statements, which fights with the
///     "return the connection as soon as the result is materialised"
///     policy.  Transactional work uses `IDatabaseConnectionPool`
///     directly.

#pragma once

#include "Services/Persistence/Database/IDatabaseConnectionPool.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Persistence::Database {

// ─── Parameter and result types ─────────────────────────────────────────────

/// Single parameter bound to a query placeholder.  The backend does
/// its own escaping; callers just hand over strings.  Typed
/// parameters (int, bool, bytea) are represented by stringified
/// values plus a hint, matching how libpq's binary parameters work.
struct QueryParameter
{
    std::string value;
    enum class Kind : std::uint8_t { Text = 0, Integer, Real, Boolean, Binary };
    Kind        kind = Kind::Text;
};

/// Column metadata returned alongside the row data.
struct QueryColumn
{
    std::string name;
    std::uint32_t oid = 0; ///< Backend-specific type oid.
};

/// One row, stored as a flat vector of column values.  Order matches
/// `QueryResult::columns`.  Null values are represented by an empty
/// string with the `isNull` flag set — this saves us an `optional`
/// per cell on hot paths where most values are non-null.
struct QueryRow
{
    std::vector<std::string> values;
    std::vector<bool>        isNull;
};

/// Complete result payload.  `rowsAffected` is meaningful for DML
/// statements; SELECTs typically leave it at zero and fill `rows`.
struct QueryResult
{
    std::vector<QueryColumn> columns;
    std::vector<QueryRow>    rows;
    std::uint64_t            rowsAffected = 0;
};

// ─── Status codes ───────────────────────────────────────────────────────────

/// Outcome of an async query.  Distinct from `AcquireResult` because
/// the query can succeed at the pool layer and still fail at the
/// backend layer (syntax error, constraint violation, deadlock).
enum class QueryStatus : std::uint8_t
{
    Pending = 0,
    Ok,
    Cancelled,
    PoolExhausted,
    BackendError,      ///< Query reached the backend and was rejected.
    Timeout,
    ExecutorShutdown,
    InternalError,
};

// ─── Handle ─────────────────────────────────────────────────────────────────

/// Future-like handle returned by `Submit`.  Thread-safe for read;
/// writes are owned by the worker thread executing the query.
/// Shared ownership lets callers pass the handle across subsystems
/// without worrying about lifetime.
class AsyncQueryHandle
{
public:
    AsyncQueryHandle() = default;

    /// True once the query has finished, successfully or not.  Safe
    /// to poll from any thread.
    [[nodiscard]] bool IsReady() const noexcept
    {
        return status_.load(std::memory_order_acquire) != QueryStatus::Pending;
    }

    [[nodiscard]] QueryStatus Status() const noexcept
    {
        return status_.load(std::memory_order_acquire);
    }

    /// Result payload.  Only valid to read after `IsReady()` returns
    /// true; the worker thread stops touching it at that point.
    [[nodiscard]] const QueryResult& Result() const noexcept { return result_; }

    /// Operator-facing error message when `Status() != Ok`.  Empty on
    /// success paths — the hot loop should check `Status` and branch.
    [[nodiscard]] const std::string& ErrorMessage() const noexcept { return errorMessage_; }

    /// Request cancellation.  Cooperative: workers that have not yet
    /// dequeued the query honour it; workers mid-query finish and
    /// discard.
    void Cancel() noexcept { cancelRequested_.store(true, std::memory_order_release); }

    [[nodiscard]] bool CancelRequested() const noexcept
    {
        return cancelRequested_.load(std::memory_order_acquire);
    }

    // ── Internal ─────────────────────────────────────────────────────────
    // The worker calls these to publish the final state.  Split from
    // the public interface so the worker can update fields atomically
    // without racing with observers.

    void SetResult(QueryResult&& result) noexcept
    {
        result_ = std::move(result);
        status_.store(QueryStatus::Ok, std::memory_order_release);
    }
    void SetFailure(QueryStatus status, std::string message) noexcept
    {
        errorMessage_ = std::move(message);
        status_.store(status, std::memory_order_release);
    }

private:
    std::atomic<QueryStatus> status_{QueryStatus::Pending};
    std::atomic<bool>        cancelRequested_{false};
    QueryResult              result_;
    std::string              errorMessage_;
};

using AsyncQueryHandlePtr = std::shared_ptr<AsyncQueryHandle>;

// ─── Submission descriptor ──────────────────────────────────────────────────

/// Everything needed to run one query.  Grouped into a struct so new
/// fields (timeout hints, priority classes) can be added without
/// breaking every call site.
struct AsyncQueryRequest
{
    std::string                 sql;
    std::vector<QueryParameter> parameters;

    /// Per-request timeout in milliseconds.  Zero = fall back to the
    /// executor's default.  Enforced by a watchdog in the executor,
    /// not by the backend, so the behaviour is identical across
    /// backends that lack a native statement timeout.
    std::uint32_t timeoutMs = 0;

    /// Free-form tag for the logs and metrics pipeline.  Always set
    /// this — generic "unknown" queries are impossible to triage when
    /// they start failing.
    std::string diagnosticsTag;
};

// ─── Interface ──────────────────────────────────────────────────────────────

/// Abstract executor.  One instance per backend is typical; stress
/// tests may spin up several with different thread counts to isolate
/// a hot path from background bookkeeping.
class IAsyncQueryExecutor
{
public:
    virtual ~IAsyncQueryExecutor() = default;

    /// Start the worker threads backing this executor.  `poolHandle`
    /// is shared — the executor does not own the pool, because the
    /// same pool typically serves several services (query executor,
    /// migration runner, presence tracker).
    [[nodiscard]] virtual bool Start(
        DatabasePoolPtr poolHandle,
        std::uint32_t   workerThreadCount) = 0;

    /// Tear down the executor, waiting for in-flight queries.  After
    /// this call returns, any pending handles settle to
    /// `ExecutorShutdown`.
    virtual void Stop() = 0;

    /// Submit a query.  The returned handle is always non-null; the
    /// caller polls or waits on it to observe completion.  If the
    /// executor is already stopped the handle publishes
    /// `ExecutorShutdown` immediately.
    [[nodiscard]] virtual AsyncQueryHandlePtr Submit(
        const AsyncQueryRequest& request) = 0;

    /// Running count of queries currently in the queue or executing.
    /// Cheap to read; intended for the backpressure logic inside
    /// services that care about avoiding a runaway queue.
    [[nodiscard]] virtual std::uint32_t PendingCount() const noexcept = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using AsyncQueryExecutorPtr = std::shared_ptr<IAsyncQueryExecutor>;

} // namespace SagaEngine::Persistence::Database
