/// @file ResourceManager.h
/// @brief Reference-counted residency cache on top of `StreamingManager`.
///
/// Layer  : SagaEngine / Resources
/// Purpose: `StreamingManager` turns one request into one decoded
///          payload.  It does not remember anything: two callers
///          asking for the same mesh in the same frame end up
///          fetching the same bytes twice.  `ResourceManager` is
///          the layer that fixes that.  It owns a residency map
///          keyed by `AssetId`, hands out reference-counted
///          handles, and evicts cold entries once total resident
///          bytes exceed a configurable budget.  Every consumer
///          that wants a mesh, a texture, or an audio clip asks
///          *the manager* instead of *the streaming system*, and
///          the manager transparently routes cache misses through
///          `StreamingManager::Submit` while cache hits never pay
///          a worker-thread round trip.
///
/// Design:
///   - Refcount, not GC.  Each `ResidencyHandle` bumps the entry's
///     refcount on construction and drops it on destruction.
///     An entry with zero refs becomes eligible for eviction but
///     is not discarded immediately — keeping it around lets a
///     later `Acquire` with the same id reuse the bytes instead
///     of re-streaming them.
///   - LRU eviction.  A doubly-linked list of zero-ref entries,
///     ordered by last-release time, is walked on every
///     `Acquire` that would push total bytes past the budget.
///     The scan stops as soon as the freed bytes are enough, so
///     quiescent periods do nothing.
///   - No double-streaming.  If an `Acquire` lands on an entry
///     whose `StreamingRequestHandle` is still `Pending`, the
///     new caller gets the *same* handle; streaming does not run
///     twice.  This is the entire point of the residency map.
///   - Budgeted.  The manager tracks total resident bytes and
///     refuses new acquisitions only when the budget is zero
///     (unset) — otherwise it keeps admitting and lets eviction
///     make room.  A hard cap would cause spurious acquisition
///     failures; the caller's level of indirection is already
///     priority-based through `StreamingManager`.
///   - Thread-safe.  A single `std::mutex` guards the residency
///     map; critical sections are short (a map lookup plus a
///     list splice) so contention is low even with many
///     consumers.
///
/// What this class is NOT:
///   - Not a decoder.  Bytes come in from `StreamingManager` as
///     an opaque `std::vector<uint8_t>`; consumers that want a
///     typed view (e.g. `MeshAsset`) construct it from the bytes
///     themselves.  Generic byte storage keeps the manager free
///     of `Render/`, `Audio/`, and other dependencies.
///   - Not a file loader.  The `IAssetSource` plugged into the
///     `StreamingManager` decides where bytes come from; the
///     resource manager only sees the decoded payload.
///   - Not a texture streamer.  Mip-level residency and GPU
///     upload live in the renderer; this layer hands back CPU
///     bytes and the renderer promotes them at its own pace.

#pragma once

#include "SagaEngine/Resources/StreamingManager.h"
#include "SagaEngine/Resources/StreamingRequest.h"

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace SagaEngine::Resources {

class ResourceManager; // Forward — `ResidencyHandle` needs a back-pointer.
struct ResidencyEntry;

/// LRU list of zero-ref entries.  Declared outside the class so
/// the entry struct can store an iterator into it without a
/// dependency cycle.
using ResidencyLruList = std::list<std::shared_ptr<ResidencyEntry>>;

// ─── Residency entry ───────────────────────────────────────────────────────

/// Shared residency record.  One per cached asset; all outstanding
/// handles to the same asset point at the same `ResidencyEntry`.
/// Exposed in the header only because `ResidencyHandle` carries a
/// `shared_ptr<ResidencyEntry>`; application code never builds one
/// directly.
struct ResidencyEntry
{
    AssetId   assetId = kInvalidAssetId;
    AssetKind kind    = AssetKind::Unknown;
    std::uint8_t lod  = 0;

    /// Number of `ResidencyHandle`s that currently reference this
    /// entry.  Guarded by `ResourceManager::mutex_`; never touched
    /// without the lock held.
    std::uint32_t refCount = 0;

    /// Size of the resident bytes, in bytes.  Populated from
    /// `StreamingResultPayload::sizeBytes` at publish time.  Kept
    /// in the entry so the LRU scan does not have to reach into
    /// the payload vector.
    std::uint64_t residentBytes = 0;

    /// Streaming handle.  Populated at `Acquire` time and kept
    /// alive for the whole entry lifetime — the handle's own
    /// storage holds the decoded bytes, so as long as the entry
    /// keeps a reference the bytes stay resident.  Readers check
    /// the handle's own `IsReady` / `Status` before touching
    /// `Payload()`.
    StreamingRequestHandlePtr streamingHandle;

    /// Monotonic timestamp of the last refcount-drop-to-zero.
    /// Used to order the LRU list; populated under the lock from
    /// `ResourceManager::NextTick`.
    std::uint64_t lastReleasedTick = 0;

    /// Iterator into `ResourceManager::lru_`, valid only while
    /// `inLru` is true.  Storing the iterator on the entry lets
    /// eviction run O(1) splice / erase without a secondary map.
    ResidencyLruList::iterator lruIt{};
    bool                       inLru = false;
};

using ResidencyEntryPtr = std::shared_ptr<ResidencyEntry>;

// ─── Residency handle ──────────────────────────────────────────────────────

/// Reference-counted borrow of a resident asset.  Obtained from
/// `ResourceManager::Acquire` and released by destruction or by
/// `Reset`.  Copying a handle bumps the refcount; moving it
/// transfers ownership without touching the counter.
class ResidencyHandle
{
public:
    ResidencyHandle() noexcept = default;

    /// Test whether the handle is bound to an entry.  A
    /// default-constructed or reset handle is empty; a handle
    /// returned from a failed `Acquire` is also empty.
    [[nodiscard]] bool IsValid() const noexcept { return entry_ != nullptr; }

    /// Test whether the underlying streaming request has completed.
    /// An `IsValid` handle may still be `!IsReady` while the bytes
    /// are still in flight on a worker thread.
    [[nodiscard]] bool IsReady() const noexcept;

    /// Terminal streaming status observed on the entry.  `Pending`
    /// if the request has not finished yet, `Ok` on success,
    /// anything else on failure.
    [[nodiscard]] StreamingStatus Status() const noexcept;

    [[nodiscard]] AssetId      Id()   const noexcept;
    [[nodiscard]] AssetKind    Kind() const noexcept;
    [[nodiscard]] std::uint8_t Lod()  const noexcept;

    /// Bytes view.  Empty span while `!IsReady`.  The pointer is
    /// valid for the lifetime of the handle; do not hold it past
    /// `Reset` or destruction.
    [[nodiscard]] const std::uint8_t* Data()  const noexcept;
    [[nodiscard]] std::uint64_t       Size()  const noexcept;

    /// Drop the reference explicitly.  Idempotent; safe to call on
    /// an already-empty handle.  The destructor calls this on the
    /// normal exit path, so explicit `Reset` is only needed when
    /// the caller wants to release early.
    void Reset() noexcept;

    // ── Rule of five ──────────────────────────────────────────────────────

    ResidencyHandle(const ResidencyHandle& other) noexcept;
    ResidencyHandle(ResidencyHandle&& other) noexcept;
    ResidencyHandle& operator=(const ResidencyHandle& other) noexcept;
    ResidencyHandle& operator=(ResidencyHandle&& other) noexcept;
    ~ResidencyHandle();

private:
    friend class ResourceManager;
    ResidencyHandle(ResourceManager* owner, ResidencyEntryPtr entry) noexcept
        : owner_(owner), entry_(std::move(entry)) {}

    ResourceManager*   owner_ = nullptr;
    ResidencyEntryPtr  entry_;
};

// ─── Config ────────────────────────────────────────────────────────────────

/// `ResourceManager` constructor parameters.  Defaults target a
/// desktop client with 512 MiB of asset residency budget; shipped
/// clients typically tune this per platform (console vs. mobile).
struct ResourceManagerConfig
{
    /// Soft ceiling on total resident bytes.  The LRU scan evicts
    /// zero-ref entries to keep total bytes under this cap.  Zero
    /// disables eviction entirely — useful in tests.
    std::uint64_t residentByteBudget = 512ull * 1024 * 1024;

    /// Logged one-line summary every `statsReportIntervalSeconds`.
    /// Zero disables periodic logging.  Disabled by default so
    /// tests do not spam the log.
    std::uint32_t statsReportIntervalSeconds = 0;
};

// ─── Budget snapshot ───────────────────────────────────────────────────────

/// Read-only view of residency accounting.  Returned by
/// `ResourceManager::Snapshot`.
struct ResourceResidencySnapshot
{
    std::uint64_t residentBytes   = 0;
    std::uint64_t residentBudget  = 0;
    std::uint32_t entryCount      = 0;
    std::uint32_t lruEntryCount   = 0; ///< Entries with refcount == 0 (eviction candidates).
    std::uint64_t totalAcquires   = 0;
    std::uint64_t totalCacheHits  = 0;
    std::uint64_t totalCacheMisses = 0;
    std::uint64_t totalEvictions  = 0;
};

// ─── ResourceManager ───────────────────────────────────────────────────────

/// Reference-counted asset residency cache.  One instance per
/// process; all consumers (renderer, physics, audio) share the
/// same map so the same mesh is streamed once and seen everywhere.
class ResourceManager
{
public:
    ResourceManager(StreamingManager& streaming,
                    ResourceManagerConfig config = {}) noexcept;
    ~ResourceManager();

    ResourceManager(const ResourceManager&)            = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&)                 = delete;
    ResourceManager& operator=(ResourceManager&&)      = delete;

    // ── Acquisition ────────────────────────────────────────────────────────

    /// Borrow a handle to the named asset.  On a cache hit the
    /// refcount is bumped and the handle is immediately ready
    /// (assuming a prior acquisition already completed streaming).
    /// On a cache miss the manager submits a streaming request and
    /// returns a handle whose `IsReady()` flips to true once the
    /// worker publishes the payload.
    ///
    /// `requestedLod` of zero asks for the best available LOD;
    /// non-zero values forward to the streaming manager and are
    /// honoured if the source can provide that tier.
    [[nodiscard]] ResidencyHandle Acquire(
        AssetId            assetId,
        AssetKind          kind,
        StreamingPriority  priority     = StreamingPriority::Normal,
        std::uint8_t       requestedLod = 0);

    /// Force eviction of every zero-ref entry.  Used by the
    /// application at shutdown, by tests, and by the editor when
    /// the user clicks "reload all assets".  Returns the number of
    /// entries dropped.
    std::uint32_t EvictAll();

    // ── Diagnostics ────────────────────────────────────────────────────────

    [[nodiscard]] ResourceResidencySnapshot Snapshot() const noexcept;

    /// Number of live entries (any refcount, including zero).
    [[nodiscard]] std::size_t EntryCount() const noexcept;

private:
    friend class ResidencyHandle;

    // ── Handle plumbing ──────────────────────────────────────────────────
    // Called by `ResidencyHandle`'s copy / assign / destructor so
    // the refcount tracking lives on the manager even though the
    // refcount itself is stored on the entry.
    void RetainFromHandle(const ResidencyEntryPtr& entry) noexcept;
    void ReleaseFromHandle(const ResidencyEntryPtr& entry) noexcept;

    // ── Streaming plumbing ───────────────────────────────────────────────
    // Called from the streaming manager's completion thread to
    // copy bytes out of the request handle into the residency entry
    // and wake waiters (polling handles will pick it up on their
    // next read).  Runs with `mutex_` held only briefly.
    //
    // `expectedAssetId` is captured from the entry at Submit time
    // so the completion handler can detect stale completations
    // without relying on handle pointer identity (which races with
    // the assignment of `entry->streamingHandle`).
    void OnStreamingCompletion(const ResidencyEntryPtr& entry,
                               AssetId expectedAssetId,
                               const StreamingRequestHandlePtr& handle);

    // ── LRU bookkeeping ──────────────────────────────────────────────────
    // Invariant: every entry in `lru_` has `refCount == 0`; every
    // entry with `refCount == 0` is in `lru_` exactly once.  The
    // iterator stored on the entry lets splice / erase run in O(1)
    // without searching the list.
    ResidencyLruList lru_;

    void PushToLruLocked(const ResidencyEntryPtr& entry);
    void RemoveFromLruLocked(const ResidencyEntryPtr& entry);
    void EvictToBudgetLocked();

    // ── Time ─────────────────────────────────────────────────────────────

    /// Monotonic tick counter used as an LRU timestamp.  Wall-clock
    /// ticks would work but an atomic counter is cheaper and it
    /// never goes backwards under clock skew.
    [[nodiscard]] std::uint64_t NextTick() noexcept;

    // ── State ────────────────────────────────────────────────────────────

    StreamingManager&     streaming_;
    ResourceManagerConfig config_;

    mutable std::mutex    mutex_;
    std::unordered_map<std::uint64_t, ResidencyEntryPtr> entries_;

    std::uint64_t residentBytes_ = 0;
    std::uint64_t tickCounter_   = 0;

    // Counters — all guarded by `mutex_`.
    std::uint64_t statAcquires_     = 0;
    std::uint64_t statCacheHits_    = 0;
    std::uint64_t statCacheMisses_  = 0;
    std::uint64_t statEvictions_    = 0;
};

} // namespace SagaEngine::Resources
