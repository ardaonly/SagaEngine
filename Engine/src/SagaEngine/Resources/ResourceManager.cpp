/// @file ResourceManager.cpp
/// @brief Refcounted residency, LRU eviction, and streaming integration.

#include "SagaEngine/Resources/ResourceManager.h"

#include "SagaEngine/Core/Log/Log.h"

#include <utility>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

} // namespace

// ─── ResidencyHandle ───────────────────────────────────────────────────────

bool ResidencyHandle::IsReady() const noexcept
{
    if (!entry_ || !entry_->streamingHandle)
        return false;
    return entry_->streamingHandle->IsReady();
}

StreamingStatus ResidencyHandle::Status() const noexcept
{
    if (!entry_ || !entry_->streamingHandle)
        return StreamingStatus::InternalError;
    return entry_->streamingHandle->Status();
}

AssetId ResidencyHandle::Id() const noexcept
{
    return entry_ ? entry_->assetId : kInvalidAssetId;
}

AssetKind ResidencyHandle::Kind() const noexcept
{
    return entry_ ? entry_->kind : AssetKind::Unknown;
}

std::uint8_t ResidencyHandle::Lod() const noexcept
{
    return entry_ ? entry_->lod : std::uint8_t{0};
}

const std::uint8_t* ResidencyHandle::Data() const noexcept
{
    if (!IsReady() || Status() != StreamingStatus::Ok)
        return nullptr;
    const auto& bytes = entry_->streamingHandle->Payload().bytes;
    return bytes.empty() ? nullptr : bytes.data();
}

std::uint64_t ResidencyHandle::Size() const noexcept
{
    if (!IsReady() || Status() != StreamingStatus::Ok)
        return 0;
    return entry_->streamingHandle->Payload().sizeBytes;
}

void ResidencyHandle::Reset() noexcept
{
    if (owner_ && entry_)
        owner_->ReleaseFromHandle(entry_);
    owner_ = nullptr;
    entry_.reset();
}

// ─── ResidencyHandle — rule of five ────────────────────────────────────────

ResidencyHandle::ResidencyHandle(const ResidencyHandle& other) noexcept
    : owner_(other.owner_)
    , entry_(other.entry_)
{
    if (owner_ && entry_)
        owner_->RetainFromHandle(entry_);
}

ResidencyHandle::ResidencyHandle(ResidencyHandle&& other) noexcept
    : owner_(other.owner_)
    , entry_(std::move(other.entry_))
{
    other.owner_ = nullptr;
}

ResidencyHandle& ResidencyHandle::operator=(const ResidencyHandle& other) noexcept
{
    if (this == &other)
        return *this;

    // Release the old reference first.  Self-copy is guarded above
    // so the release never drops the entry we are about to retain.
    if (owner_ && entry_)
        owner_->ReleaseFromHandle(entry_);

    owner_ = other.owner_;
    entry_ = other.entry_;

    if (owner_ && entry_)
        owner_->RetainFromHandle(entry_);

    return *this;
}

ResidencyHandle& ResidencyHandle::operator=(ResidencyHandle&& other) noexcept
{
    if (this == &other)
        return *this;

    if (owner_ && entry_)
        owner_->ReleaseFromHandle(entry_);

    owner_       = other.owner_;
    entry_       = std::move(other.entry_);
    other.owner_ = nullptr;
    return *this;
}

ResidencyHandle::~ResidencyHandle()
{
    if (owner_ && entry_)
        owner_->ReleaseFromHandle(entry_);
}

// ─── ResourceManager — construction ────────────────────────────────────────

ResourceManager::ResourceManager(StreamingManager&     streaming,
                                 ResourceManagerConfig config) noexcept
    : streaming_(streaming)
    , config_(config)
{
    LOG_INFO(kLogTag,
             "ResourceManager: budget=%llu bytes",
             static_cast<unsigned long long>(config_.residentByteBudget));
}

ResourceManager::~ResourceManager()
{
    // Nothing explicit to tear down: every `ResidencyHandle` that is
    // still alive owns a shared_ptr to its entry, and the entries'
    // `streamingHandle` members keep the streaming payloads alive
    // until every consumer has let go.  This is intentional —
    // destroying the manager out from under live handles would
    // leave dangling pointers inside them and we would rather the
    // owner fix the lifetime bug than paper over it with a join.
    const std::size_t live = entries_.size();
    if (live != 0)
    {
        LOG_WARN(kLogTag,
                 "ResourceManager: %zu entries still live at destruction", live);
    }
}

// ─── Acquire ───────────────────────────────────────────────────────────────

ResidencyHandle ResourceManager::Acquire(AssetId            assetId,
                                         AssetKind          kind,
                                         StreamingPriority  priority,
                                         std::uint8_t       requestedLod)
{
    if (assetId == kInvalidAssetId || kind == AssetKind::Unknown)
    {
        LOG_WARN(kLogTag,
                 "ResourceManager::Acquire: invalid request (assetId=%llu kind=%u)",
                 static_cast<unsigned long long>(assetId),
                 static_cast<unsigned>(kind));
        return {};
    }

    ResidencyEntryPtr        entry;
    StreamingRequest         submitRequest;
    bool                     needStream = false;

    {
        std::lock_guard lock(mutex_);
        ++statAcquires_;

        auto it = entries_.find(assetId);
        if (it != entries_.end())
        {
            entry = it->second;
            ++statCacheHits_;

            // If the entry was sitting in the LRU list waiting to
            // be evicted, pull it out: a new reference means it's
            // live again and must not appear as an eviction
            // candidate.
            if (entry->inLru)
                RemoveFromLruLocked(entry);

            ++entry->refCount;
        }
        else
        {
            ++statCacheMisses_;

            entry           = std::make_shared<ResidencyEntry>();
            entry->assetId  = assetId;
            entry->kind     = kind;
            entry->lod      = requestedLod;
            entry->refCount = 1;

            entries_.emplace(assetId, entry);

            submitRequest.assetId      = assetId;
            submitRequest.kind         = kind;
            submitRequest.priority     = priority;
            submitRequest.requestedLod = requestedLod;
            needStream                 = true;
        }
    }

    // Submit streaming outside of the lock.  `Submit` may take its
    // own mutex and may fire the completion callback inline on a
    // rejection path; running it with ours held is a deadlock
    // hazard.
    if (needStream)
    {
        // Weak pointer keeps the completion callback from
        // resurrecting the entry if every caller has already let
        // go: if we deleted the entry during a fast acquire /
        // release dance, we do not want streaming completion to
        // reinsert it.
        std::weak_ptr<ResidencyEntry> weakEntry = entry;

        // Capture the entry's assetId so the completion handler
        // can verify it is still dealing with the same request
        // even if the entry has been re-armed with a new handle.
        const AssetId capturedAssetId = entry->assetId;

        auto callback = [this, weakEntry, capturedAssetId](const StreamingRequestHandlePtr& handle) {
            if (auto locked = weakEntry.lock())
                OnStreamingCompletion(locked, capturedAssetId, handle);
        };

        auto streamingHandle = streaming_.Submit(std::move(submitRequest),
                                                 std::move(callback));

        // Attach the handle to the entry.  The completion callback
        // uses the captured `capturedAssetId` to verify it is
        // completing the same request that was submitted, so there
        // is no race even if the worker finishes before this
        // assignment runs.
        {
            std::lock_guard lock(mutex_);
            entry->streamingHandle = std::move(streamingHandle);
        }
    }

    return ResidencyHandle{this, std::move(entry)};
}

// ─── Handle plumbing ───────────────────────────────────────────────────────

void ResourceManager::RetainFromHandle(const ResidencyEntryPtr& entry) noexcept
{
    if (!entry)
        return;
    std::lock_guard lock(mutex_);

    // A retain that lifts the refcount above zero must pull the
    // entry back out of the LRU list, otherwise a later eviction
    // pass would mistake it for an inactive entry.
    if (entry->refCount == 0 && entry->inLru)
        RemoveFromLruLocked(entry);

    ++entry->refCount;
}

void ResourceManager::ReleaseFromHandle(const ResidencyEntryPtr& entry) noexcept
{
    if (!entry)
        return;
    std::lock_guard lock(mutex_);

    if (entry->refCount == 0)
    {
        LOG_ERROR(kLogTag,
                  "ResourceManager: refcount underflow on assetId=%llu",
                  static_cast<unsigned long long>(entry->assetId));
        return;
    }

    --entry->refCount;
    if (entry->refCount != 0)
        return;

    // Last reference — move the entry to the LRU list.  It stays
    // resident for now so a follow-up `Acquire` is a cheap cache
    // hit; eviction only fires when a later acquisition pushes the
    // total resident bytes past the budget.
    entry->lastReleasedTick = NextTick();
    PushToLruLocked(entry);
}

// ─── Streaming completion ──────────────────────────────────────────────────

void ResourceManager::OnStreamingCompletion(const ResidencyEntryPtr&         entry,
                                            AssetId                          expectedAssetId,
                                            const StreamingRequestHandlePtr& handle)
{
    if (!entry || !handle)
        return;

    // Verify this completion is for the request we expect.  If the
    // entry was re-armed with a different asset (e.g. a new Acquire
    // for a different mesh after the old one was evicted), drop the
    // stale completion so we don't attribute bytes to the wrong asset.
    if (entry->assetId != expectedAssetId)
        return;

    const StreamingStatus status = handle->Status();
    const std::uint64_t   bytes  = (status == StreamingStatus::Ok)
                                     ? handle->Payload().sizeBytes
                                     : 0;

    {
        std::lock_guard lock(mutex_);

        entry->residentBytes = bytes;
        residentBytes_ += bytes;

        if (status != StreamingStatus::Ok)
        {
            LOG_WARN(kLogTag,
                     "ResourceManager: streaming failed for assetId=%llu status=%u",
                     static_cast<unsigned long long>(entry->assetId),
                     static_cast<unsigned>(status));
        }

        EvictToBudgetLocked();
    }
}

// ─── LRU bookkeeping ───────────────────────────────────────────────────────

void ResourceManager::PushToLruLocked(const ResidencyEntryPtr& entry)
{
    if (entry->inLru)
        return;
    entry->lruIt = lru_.insert(lru_.end(), entry);
    entry->inLru = true;
}

void ResourceManager::RemoveFromLruLocked(const ResidencyEntryPtr& entry)
{
    if (!entry->inLru)
        return;
    lru_.erase(entry->lruIt);
    entry->inLru = false;
    entry->lruIt = ResidencyLruList::iterator{};
}

void ResourceManager::EvictToBudgetLocked()
{
    // Budget of zero means "no eviction" — useful in tests that
    // want to inspect the residency map without it shifting
    // underneath them.
    if (config_.residentByteBudget == 0)
        return;

    // Evict from the front of the LRU list (oldest released
    // timestamp) until resident bytes fit the budget again.  The
    // list is ordered by release time because `PushToLruLocked`
    // always appends to the back.
    while (residentBytes_ > config_.residentByteBudget && !lru_.empty())
    {
        auto victim = lru_.front();
        lru_.pop_front();
        victim->inLru = false;
        victim->lruIt = ResidencyLruList::iterator{};

        const std::uint64_t freed = victim->residentBytes;
        if (freed > residentBytes_)
            residentBytes_ = 0; // defensive — should never fire
        else
            residentBytes_ -= freed;

        entries_.erase(victim->assetId);
        ++statEvictions_;
    }
}

std::uint32_t ResourceManager::EvictAll()
{
    std::uint32_t evicted = 0;
    std::lock_guard lock(mutex_);

    while (!lru_.empty())
    {
        auto victim = lru_.front();
        lru_.pop_front();
        victim->inLru = false;
        victim->lruIt = ResidencyLruList::iterator{};

        if (victim->residentBytes > residentBytes_)
            residentBytes_ = 0;
        else
            residentBytes_ -= victim->residentBytes;

        entries_.erase(victim->assetId);
        ++evicted;
    }

    statEvictions_ += evicted;
    return evicted;
}

// ─── Snapshot ──────────────────────────────────────────────────────────────

ResourceResidencySnapshot ResourceManager::Snapshot() const noexcept
{
    std::lock_guard lock(mutex_);
    ResourceResidencySnapshot s;
    s.residentBytes    = residentBytes_;
    s.residentBudget   = config_.residentByteBudget;
    s.entryCount       = static_cast<std::uint32_t>(entries_.size());
    s.lruEntryCount    = static_cast<std::uint32_t>(lru_.size());
    s.totalAcquires    = statAcquires_;
    s.totalCacheHits   = statCacheHits_;
    s.totalCacheMisses = statCacheMisses_;
    s.totalEvictions   = statEvictions_;
    return s;
}

std::size_t ResourceManager::EntryCount() const noexcept
{
    std::lock_guard lock(mutex_);
    return entries_.size();
}

// ─── NextTick ──────────────────────────────────────────────────────────────

std::uint64_t ResourceManager::NextTick() noexcept
{
    // Caller already holds `mutex_` — this function is intentionally
    // lock-free so it can be invoked in the middle of a locked
    // section without re-entering.
    return ++tickCounter_;
}

} // namespace SagaEngine::Resources
