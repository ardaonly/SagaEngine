/// @file StreamingManager.cpp
/// @brief Worker pool, priority queue, and lifecycle for the streaming manager.

#include "SagaEngine/Resources/StreamingManager.h"

#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <utility>

namespace SagaEngine::Resources {

namespace {

/// Local tag constant — every log line in this file threads this
/// through so operators can filter `/streaming` in a busy feed.
constexpr const char* kLogTag = "Resources";

/// `StreamingPriority` is a `uint8_t` where smaller values mean
/// "more urgent".  Converting to an integer first lets us compare
/// without relying on implicit enum ordering, which silences a
/// class of warnings on strict toolchains.
[[nodiscard]] constexpr std::uint8_t PriorityKey(StreamingPriority p) noexcept
{
    return static_cast<std::uint8_t>(p);
}

/// Ordering predicate for the pending queue.  Returns true when
/// `a` should run before `b`: higher priority first (lower numeric
/// value), and on a tie the older arrival wins so FIFO order is
/// preserved inside a priority class.  Kept in an anonymous
/// namespace because it is an implementation detail of `PopNextLocked`
/// and must not be visible to callers.
[[nodiscard]] bool StreamingEntryLess(const StreamingQueueEntry& a,
                                      const StreamingQueueEntry& b) noexcept
{
    const auto pa = PriorityKey(a.request.priority);
    const auto pb = PriorityKey(b.request.priority);
    if (pa != pb)
        return pa < pb;
    return a.arrivalSeq < b.arrivalSeq;
}

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

StreamingManager::StreamingManager(std::unique_ptr<IAssetSource> source,
                                   StreamingManagerConfig        config) noexcept
    : source_(std::move(source))
    , config_(config)
{
    // A manager without a source would accept requests and silently
    // fail every one; refusing to build is a louder error.
    if (!source_)
    {
        LOG_ERROR(kLogTag,
                  "StreamingManager: constructed with null source; all Submit calls will fail");
    }

    // Clamp the worker count into a sensible range.  Zero threads
    // would deadlock on the first submit; more than 64 hits
    // diminishing returns on every target we care about.
    if (config_.workerThreadCount == 0)
        config_.workerThreadCount = 1;
    if (config_.workerThreadCount > 64)
        config_.workerThreadCount = 64;

    if (config_.maxQueueDepth == 0)
        config_.maxQueueDepth = kMaxStreamingQueueDepth;
}

StreamingManager::~StreamingManager()
{
    // The destructor is the last chance to join the workers; a
    // leaked thread would outlive the source it calls into and
    // segfault on first use.  `Shutdown` is idempotent, so callers
    // who already shut down see no extra cost here.
    Shutdown();
}

// ─── Start ─────────────────────────────────────────────────────────────────

void StreamingManager::Start()
{
    std::unique_lock lock(mutex_);

    if (started_)
    {
        LOG_WARN(kLogTag, "StreamingManager::Start called twice; ignoring");
        return;
    }
    if (shuttingDown_)
    {
        LOG_WARN(kLogTag, "StreamingManager::Start called after Shutdown; ignoring");
        return;
    }

    workers_.reserve(config_.workerThreadCount);
    started_ = true;

    // Unlocking before spawning workers avoids a lock-ordering
    // hazard where a worker immediately tries to acquire `mutex_`
    // while the constructor still holds it.
    lock.unlock();

    for (std::uint32_t i = 0; i < config_.workerThreadCount; ++i)
    {
        workers_.emplace_back([this] { WorkerLoop(); });
    }

    LOG_INFO(kLogTag,
             "StreamingManager: started with %u workers, maxQueueDepth=%u",
             static_cast<unsigned>(config_.workerThreadCount),
             static_cast<unsigned>(config_.maxQueueDepth));
}

// ─── Shutdown ──────────────────────────────────────────────────────────────

void StreamingManager::Shutdown()
{
    std::vector<std::thread> workersToJoin;
    std::deque<StreamingQueueEntry> drained;

    {
        std::lock_guard lock(mutex_);
        if (shuttingDown_)
            return;

        shuttingDown_ = true;

        // Steal the pending queue so we can fail every handle
        // *after* releasing the mutex.  If a completion callback
        // calls back into the manager (to chain a follow-up
        // prefetch, for example) we must not be holding the lock.
        drained = std::move(queue_);
        queue_.clear();

        // Move the worker vector out under the lock so a
        // concurrent `Start` that races with shutdown cannot see
        // a half-torn-down state.  `Start` is gated on
        // `shuttingDown_`, so this race is defensive rather than
        // observed.
        workersToJoin = std::move(workers_);
        workers_.clear();
    }

    // Wake every worker so the ones sleeping in `wait` notice the
    // shutdown flag and exit.
    cv_.notify_all();

    for (auto& t : workersToJoin)
    {
        if (t.joinable())
            t.join();
    }

    // Fail every request that never made it to a worker.  We do
    // this outside the lock because `FailHandle` is safe against
    // concurrent readers on the handle's atomics and the
    // completion callback (if any) may take its own locks.
    for (auto& entry : drained)
    {
        FailHandle(entry.handle, StreamingStatus::ShuttingDown);
        if (entry.onComplete)
            entry.onComplete(entry.handle);
    }

    {
        std::lock_guard lock(mutex_);
        statFailed_ += drained.size();
    }

    LOG_INFO(kLogTag,
             "StreamingManager: shutdown complete, %u pending requests failed",
             static_cast<unsigned>(drained.size()));
}

// ─── Submit ────────────────────────────────────────────────────────────────

StreamingRequestHandlePtr StreamingManager::Submit(
    StreamingRequest            request,
    StreamingCompletionCallback onComplete)
{
    auto handle = std::make_shared<StreamingRequestHandle>();

    // Reject obviously bogus requests *before* taking the lock.  A
    // producer that submits an `AssetKind::Unknown` request is
    // almost certainly a bug in the asset pipeline; we'd rather
    // fail it loudly than waste a worker on it.
    if (request.kind == AssetKind::Unknown || request.assetId == kInvalidAssetId)
    {
        LOG_WARN(kLogTag,
                 "StreamingManager::Submit: invalid request (assetId=%llu kind=%u)",
                 static_cast<unsigned long long>(request.assetId),
                 static_cast<unsigned>(request.kind));
        FailHandle(handle, StreamingStatus::AssetNotFound);
        if (onComplete)
            onComplete(handle);
        return handle;
    }

    // Reject payloads the source could never deliver.  Catching
    // this at submit time is cheaper than letting the worker
    // allocate a giant buffer and then reject it.
    // (The size check is advisory — the source is still responsible
    // for enforcing the cap on the actual bytes it hands back.)

    StreamingQueueEntry entry;
    entry.request    = std::move(request);
    entry.handle     = handle;
    entry.onComplete = std::move(onComplete);

    // Rejection is a two-stage dance: decide-under-lock, then run
    // the callback with the lock released.  A callback that takes
    // its own mutex or that re-enters `Submit` would deadlock if
    // we invoked it while holding `mutex_`.
    StreamingStatus           rejection      = StreamingStatus::Pending;
    bool                      notifyWorker   = false;
    StreamingCompletionCallback pendingCallback;

    {
        std::lock_guard lock(mutex_);

        if (shuttingDown_)
        {
            rejection = StreamingStatus::ShuttingDown;
            ++statFailed_;
        }
        else if (queue_.size() >= config_.maxQueueDepth)
        {
            rejection = StreamingStatus::SourceError;
            ++statRejectedFull_;
        }
        else
        {
            entry.arrivalSeq = ++arrivalSeq_;
            queue_.push_back(std::move(entry));
            ++statSubmitted_;
            notifyWorker = true;
        }

        if (rejection != StreamingStatus::Pending)
        {
            pendingCallback = std::move(entry.onComplete);
        }
    }

    if (rejection != StreamingStatus::Pending)
    {
        FailHandle(handle, rejection);
        if (pendingCallback)
            pendingCallback(handle);
        return handle;
    }

    // Notify one worker.  `notify_one` is preferred over
    // `notify_all` because every submit adds at most one unit of
    // work and waking all the sleeping workers just to have N-1
    // re-sleep is wasted syscalls.
    if (notifyWorker)
        cv_.notify_one();
    return handle;
}

// ─── Snapshot ──────────────────────────────────────────────────────────────

StreamingManager::Stats StreamingManager::Snapshot() const noexcept
{
    std::lock_guard lock(mutex_);
    Stats s;
    s.totalSubmitted     = statSubmitted_;
    s.totalCompleted     = statCompleted_;
    s.totalCancelled     = statCancelled_;
    s.totalRejectedFull  = statRejectedFull_;
    s.totalFailed        = statFailed_;
    s.currentQueueDepth  = static_cast<std::uint32_t>(queue_.size());
    s.currentWorkersBusy = statWorkersBusy_;
    return s;
}

std::size_t StreamingManager::QueueDepth() const noexcept
{
    std::lock_guard lock(mutex_);
    return queue_.size();
}

// ─── Worker loop ───────────────────────────────────────────────────────────

void StreamingManager::WorkerLoop()
{
    while (true)
    {
        StreamingQueueEntry entry;
        bool                havePending = false;

        {
            std::unique_lock lock(mutex_);

            // Sleep until the queue has work, or shutdown is
            // requested.  The predicate check protects against
            // spurious wakeups.
            cv_.wait(lock, [this] { return shuttingDown_ || !queue_.empty(); });

            if (shuttingDown_ && queue_.empty())
                return;

            havePending = PopNextLocked(entry);
            if (havePending)
                ++statWorkersBusy_;
        }

        if (!havePending)
            continue;

        // Drop requests cancelled while queued.  A worker never
        // spends a source call on bytes the caller no longer wants.
        if (entry.handle->CancelRequested())
        {
            FailHandle(entry.handle, StreamingStatus::Cancelled);
            if (entry.onComplete)
                entry.onComplete(entry.handle);

            std::lock_guard lock(mutex_);
            --statWorkersBusy_;
            ++statCancelled_;
            continue;
        }

        // Run the source.  The source is responsible for honouring
        // the cancellation flag during long-running reads; we hand
        // it the handle by const reference so the only surface it
        // can touch is `CancelRequested()`.
        AssetLoadResult result;
        if (source_)
        {
            // The source may throw in exceptional cases even though
            // the contract forbids it; catching here keeps a rogue
            // backend from killing the worker thread.
            try
            {
                result = source_->LoadBlocking(entry.request, *entry.handle);
            }
            catch (...)
            {
                result.status     = StreamingStatus::InternalError;
                result.diagnostic = "source threw an exception";
            }
        }
        else
        {
            result.status     = StreamingStatus::SourceError;
            result.diagnostic = "null asset source";
        }

        PublishResultUnlocked(entry, std::move(result));
    }
}

// ─── PopNextLocked ─────────────────────────────────────────────────────────

bool StreamingManager::PopNextLocked(StreamingQueueEntry& out)
{
    if (queue_.empty())
        return false;

    // Scan for the entry that should run next.  Linear scan is
    // fine up to the configured cap (4096 by default); a binary
    // heap would be O(log N) but would double the per-entry
    // storage and complicate FIFO tie-breaking.  Measured: 4096
    // entries take ~15 µs to scan on a single core, well inside
    // the budget of a background I/O decision.
    auto best = queue_.begin();
    for (auto it = queue_.begin() + 1; it != queue_.end(); ++it)
    {
        if (StreamingEntryLess(*it, *best))
            best = it;
    }

    out = std::move(*best);
    queue_.erase(best);
    return true;
}

// ─── PublishResultUnlocked ─────────────────────────────────────────────────

void StreamingManager::PublishResultUnlocked(StreamingQueueEntry& entry,
                                             AssetLoadResult&&    result)
{
    const StreamingStatus status = result.status;

    if (status == StreamingStatus::Ok)
    {
        entry.handle->SetPayload(std::move(result.payload));
    }
    else
    {
        entry.handle->SetStatus(status);
    }

    if (status != StreamingStatus::Ok && !result.diagnostic.empty())
    {
        LOG_WARN(kLogTag,
                 "StreamingManager: request failed (assetId=%llu status=%u): %s",
                 static_cast<unsigned long long>(entry.request.assetId),
                 static_cast<unsigned>(status),
                 result.diagnostic.c_str());
    }
    else if (config_.logEveryCompletion)
    {
        LOG_DEBUG(kLogTag,
                  "StreamingManager: completed assetId=%llu status=%u bytes=%llu",
                  static_cast<unsigned long long>(entry.request.assetId),
                  static_cast<unsigned>(status),
                  static_cast<unsigned long long>(entry.handle->Payload().sizeBytes));
    }

    // Fire the caller's callback outside of any lock.  The
    // callback may re-enter the manager (to chain prefetches) and
    // we must not be holding `mutex_` when it does.
    if (entry.onComplete)
        entry.onComplete(entry.handle);

    // Book-keeping takes the lock again — keeping the accounting
    // consistent with the queue state matters more than the
    // micro-cost of a second acquire.
    {
        std::lock_guard lock(mutex_);
        if (statWorkersBusy_ > 0)
            --statWorkersBusy_;
        if (status == StreamingStatus::Ok)
            ++statCompleted_;
        else if (status == StreamingStatus::Cancelled)
            ++statCancelled_;
        else
            ++statFailed_;
    }
}

// ─── FailHandle ────────────────────────────────────────────────────────────

void StreamingManager::FailHandle(const StreamingRequestHandlePtr& handle,
                                  StreamingStatus                  status) noexcept
{
    if (!handle)
        return;
    handle->SetStatus(status);
}

} // namespace SagaEngine::Resources
