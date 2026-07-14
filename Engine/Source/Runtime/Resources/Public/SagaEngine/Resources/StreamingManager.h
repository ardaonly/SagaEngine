/// @file StreamingManager.h
/// @brief Priority-ordered asset streaming orchestrator with worker pool.
///
/// Layer  : SagaEngine / Resources
/// Purpose: The streaming manager is the only component in the
///          resources subsystem that talks to threads and to the
///          real clock.  Its job is to accept `StreamingRequest`s
///          from the renderer, the physics streamer, the audio
///          engine, and the world prefetcher, forward each one to
///          an `IAssetSource`, and publish the decoded payload
///          back into the caller's `StreamingRequestHandle` with
///          strict ordering guarantees.  Nothing else in the engine
///          owns a thread for asset I/O — everything funnels
///          through this class so residency accounting, cancellation,
///          and shutdown are centralised.
///
/// Design:
///   - A single priority queue sorted by `(priority, arrivalSeq)`.
///     `arrivalSeq` is a monotonic counter incremented on every
///     `Submit`, so equal-priority requests fall back to FIFO
///     order.  This is the standard "stable priority queue" trick
///     that avoids starvation within a class.
///   - A small worker thread pool (default four threads, configurable)
///     drains the queue.  Workers block on a condition variable
///     while the queue is empty and wake up on `Submit` or on
///     shutdown.  Four threads is enough to keep a spinning-rust
///     disk saturated without thrashing the page cache; more
///     makes sense only for a network source, and that tuning
///     lives in the config rather than in the code.
///   - Cancellation is cooperative.  `Cancel()` on a public handle
///     flips the handle's atomic flag; workers observe it on the
///     next poll and return `StreamingStatus::Cancelled`.  Queued
///     requests that have not yet been picked up are dropped at
///     dispatch time so cancelled background prefetches do not
///     block a critical request behind them.
///   - Shutdown is two-phase.  `Shutdown()` first sets the
///     shutting-down flag so `Submit` rejects with
///     `StreamingStatus::ShuttingDown`, then notifies every worker
///     to exit, joins them, and finally walks the queue to
///     publish `StreamingStatus::ShuttingDown` on every handle
///     still pending.  The rule: no handle is left in the
///     `Pending` state after `Shutdown` returns.
///   - Bounded queue.  `kMaxStreamingQueueDepth` caps the total
///     outstanding requests.  A runaway producer that keeps
///     submitting faster than workers can drain gets
///     `StreamingStatus::SourceError` on the rejected handle so
///     the bug is immediately visible rather than manifesting as
///     a memory leak hours later.
///
/// What this class is NOT:
///   - Not a decoded asset cache.  `StreamingManager` produces
///     bytes and moves them into the caller's handle; the
///     reference-counted residency map that deduplicates requests
///     and evicts under memory pressure lives in `ResourceManager`
///     (still open in the roadmap).
///   - Not an I/O backend.  Disk, package, and network readers are
///     plugged in behind `IAssetSource` — the manager never
///     imports `<fstream>` or `<sys/socket.h>`.
///   - Not a scheduler for GPU upload.  Once the bytes are in the
///     handle, the renderer decides when and how to hand them to
///     the RHI.  The manager's job finishes at the handle.

#pragma once

#include "SagaEngine/Resources/IAssetSource.h"
#include "SagaEngine/Resources/StreamingRequest.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace SagaEngine::Resources {

// ─── Config ────────────────────────────────────────────────────────────────

/// Constructor parameters.  Defaults target a desktop client reading
/// from a loose-files source during development; shipped builds tune
/// `workerThreadCount` up when the source is a package file or a CDN
/// and can keep more parallel I/O in flight.
struct StreamingManagerConfig
{
    /// Number of worker threads draining the queue.  Must be at
    /// least one; four is a good default for mixed SSD / HDD targets.
    std::uint32_t workerThreadCount = 4;

    /// Hard ceiling on queued requests.  A producer that submits past
    /// this cap gets an immediate `StreamingStatus::SourceError` on
    /// its handle instead of silently consuming more memory.  The
    /// default comes from `StreamingRequest.h`.
    std::uint32_t maxQueueDepth = kMaxStreamingQueueDepth;

    /// When true, the manager logs one line per completed request at
    /// `LOG_DEBUG` level.  Off by default because a busy client can
    /// issue thousands of requests per second and the noise drowns
    /// other diagnostics.
    bool logEveryCompletion = false;
};

// ─── Queue entry ───────────────────────────────────────────────────────────

/// Internal queue entry.  Exposed in the header only because the
/// tests poke at it directly; normal callers interact through
/// `Submit` and the returned handle and never touch this type.
struct StreamingQueueEntry
{
    StreamingRequest            request;
    StreamingRequestHandlePtr   handle;
    StreamingCompletionCallback onComplete;

    /// Monotonic counter from `StreamingManager::arrivalSeq_`.  Used
    /// to break ties between equal-priority requests so the FIFO
    /// invariant within a class holds regardless of std container
    /// ordering.
    std::uint64_t arrivalSeq = 0;
};

// ─── StreamingManager ──────────────────────────────────────────────────────

/// Priority-ordered streaming orchestrator.  One instance per
/// process; the application wires it up at boot and hands it to
/// every subsystem that needs asset I/O.
class StreamingManager
{
public:
    /// Build a manager with an explicit source.  Ownership of the
    /// source transfers to the manager; the caller keeps no
    /// reference.  The manager does not start its workers until
    /// `Start()` is called so test code can inject a source and
    /// then exercise `Submit` synchronously if it wants.
    StreamingManager(std::unique_ptr<IAssetSource> source,
                     StreamingManagerConfig       config = {}) noexcept;

    ~StreamingManager();

    StreamingManager(const StreamingManager&)            = delete;
    StreamingManager& operator=(const StreamingManager&) = delete;
    StreamingManager(StreamingManager&&)                 = delete;
    StreamingManager& operator=(StreamingManager&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /// Spawn the worker threads.  Safe to call once; a second call
    /// is a no-op and logs a warning.  Must precede the first
    /// `Submit` that expects asynchronous completion.
    void Start();

    /// Stop the workers and fail every still-pending handle with
    /// `StreamingStatus::ShuttingDown`.  Idempotent — the second
    /// call is a no-op.  Called from the destructor, so an
    /// explicit call is only required when the caller wants to
    /// tear the manager down before its owning scope exits.
    void Shutdown();

    // ── Submission ────────────────────────────────────────────────────────

    /// Enqueue a request and return a handle the caller can poll,
    /// cancel, or attach a completion callback to.  Never returns
    /// a null pointer: on rejection (queue full, invalid `kind`,
    /// manager shutting down) the returned handle carries a
    /// non-`Pending` status the caller can inspect.
    ///
    /// Threading: safe to call from any thread, including inside a
    /// completion callback (the manager takes care not to hold
    /// its mutex while invoking callbacks).
    [[nodiscard]] StreamingRequestHandlePtr Submit(
        StreamingRequest            request,
        StreamingCompletionCallback onComplete = {});

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Read-only counters snapshot.  Safe to call from any thread;
    /// values are read under `mutex_` so the reported numbers are
    /// consistent with each other at the moment of capture.
    struct Stats
    {
        std::uint64_t totalSubmitted      = 0;
        std::uint64_t totalCompleted      = 0;
        std::uint64_t totalCancelled      = 0;
        std::uint64_t totalRejectedFull   = 0;
        std::uint64_t totalFailed         = 0;
        std::uint32_t currentQueueDepth   = 0;
        std::uint32_t currentWorkersBusy  = 0;
    };

    [[nodiscard]] Stats Snapshot() const noexcept;

    /// Number of requests currently waiting in the queue (not
    /// counting ones a worker has already picked up).  Primarily
    /// used by tests.
    [[nodiscard]] std::size_t QueueDepth() const noexcept;

private:
    // ─── Worker loop ──────────────────────────────────────────────────────

    /// Worker body.  Blocks on the condition variable until a
    /// request is available, pops the highest-priority entry, runs
    /// the source, and publishes the result.  Returns when
    /// `shuttingDown_` becomes true and the queue is empty.
    void WorkerLoop();

    /// Pop the highest-priority pending entry.  Must be called
    /// with `mutex_` held.  Returns false when the queue is empty;
    /// the `out` parameter is populated on true.
    bool PopNextLocked(StreamingQueueEntry& out);

    /// Publish a terminal status on a handle and fire the
    /// completion callback, if any.  Never called with `mutex_`
    /// held — callback code is untrusted and must not be able to
    /// deadlock against the manager.
    void PublishResultUnlocked(StreamingQueueEntry&   entry,
                               AssetLoadResult&&      result);

    /// Fail a handle without running the source — used when the
    /// queue is full, when submission arrives during shutdown, or
    /// when a cancelled request is popped from the queue.
    static void FailHandle(const StreamingRequestHandlePtr& handle,
                           StreamingStatus                  status) noexcept;

    // ─── State ────────────────────────────────────────────────────────────

    std::unique_ptr<IAssetSource> source_;
    StreamingManagerConfig        config_;

    mutable std::mutex              mutex_;
    std::condition_variable         cv_;
    std::deque<StreamingQueueEntry> queue_;

    std::vector<std::thread> workers_;
    bool                     started_       = false;
    bool                     shuttingDown_  = false;

    std::uint64_t arrivalSeq_ = 0;

    // ─── Counters ─────────────────────────────────────────────────────────
    // Updated under `mutex_` in every path that already holds it, so
    // the snapshot is consistent with the queue state it reports.
    std::uint64_t statSubmitted_     = 0;
    std::uint64_t statCompleted_     = 0;
    std::uint64_t statCancelled_     = 0;
    std::uint64_t statRejectedFull_  = 0;
    std::uint64_t statFailed_        = 0;
    std::uint32_t statWorkersBusy_   = 0;
};

} // namespace SagaEngine::Resources
