/// @file StreamingRequest.h
/// @brief Asynchronous asset streaming request and result contract.
///
/// Layer  : SagaEngine / Resources
/// Purpose: An MMO world is larger than the client's RAM.  Only a
///          tiny slice of assets are ever resident at once; the
///          streaming manager is the subsystem that decides which
///          mesh, texture, or audio clip should come in next, which
///          one should be evicted, and how to pace the I/O so the
///          frame rate does not hitch while a new zone loads in.
///
///          This header defines the *request* objects the rest of
///          the engine submits to the streaming manager and the
///          *result* objects the manager publishes back.  The
///          manager itself (queues, worker threads, eviction
///          policy) lives in `StreamingManager.h` — this file is
///          the data contract between producers and consumers.
///
/// Design rules:
///   - Requests are submitted by stable asset id, not by path.
///     Paths are a packaging concern; the runtime traffic in ids.
///   - Requests carry a priority class.  The streaming manager
///     sorts by priority first and by arrival time second, so a
///     "need this now" request from the renderer jumps ahead of a
///     "might need this soon" hint from the open-world prefetcher.
///   - Completion is delivered via a shared future-like handle.
///     Callers can poll, subscribe, or block depending on what fits
///     their call site.
///   - The same request type covers meshes, textures, and any
///     future asset kind.  The result payload is opaque bytes plus
///     a `kind` tag; the consumer knows what to cast to based on
///     the tag.  That keeps the streaming manager generic.
///
/// What this header is NOT:
///   - Not a file I/O layer.  The manager talks to a pluggable
///     `IAssetSource` that may read from disk, a package file, or
///     the network.  This header does not mention `fopen`.
///   - Not a memory budget tracker.  The manager tracks residency
///     internally; callers submit requests and read results.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Resources {

// ─── Asset identification ──────────────────────────────────────────────────

/// Stable asset identifier, shared with every other subsystem.  The
/// streaming manager does not care what kind of asset the id
/// references — the `kind` tag on the request disambiguates.
using AssetId = std::uint64_t;
inline constexpr AssetId kInvalidAssetId = 0;

/// Asset category tag.  Extended as new asset kinds land; unknown
/// values are rejected at submit time so a stale producer cannot
/// starve the streaming queue with garbage requests.
enum class AssetKind : std::uint8_t
{
    Unknown = 0,
    Mesh,
    Texture,
    Shader,
    Audio,
    Animation,
    MaterialAsset,
};

// ─── Priority class ────────────────────────────────────────────────────────

/// Coarse priority bucket.  Fine-grained scheduling (ties broken by
/// distance to camera, for example) happens inside the manager; the
/// request just states which bucket it belongs to.
enum class StreamingPriority : std::uint8_t
{
    Critical = 0, ///< Blocking the render thread right now.
    High     = 1, ///< Needed within a few frames.
    Normal   = 2, ///< Standard prefetch.
    Low      = 3, ///< Speculative, drop first on memory pressure.
};

// ─── Request ───────────────────────────────────────────────────────────────

/// One streaming request.  POD so submission is a straight struct
/// copy into the manager's queue — no dynamic dispatch and no
/// virtual call on the hot path.
struct StreamingRequest
{
    AssetId           assetId = kInvalidAssetId;
    AssetKind         kind    = AssetKind::Unknown;
    StreamingPriority priority = StreamingPriority::Normal;

    /// Optional requested LOD.  Zero means "best available"; higher
    /// values let the caller ask for a cheaper tier (common when a
    /// coarse LOD is "good enough" for a distant object).
    std::uint8_t requestedLod = 0;

    /// Free-form tag for logs.  Populate liberally — the manager
    /// reports these back in `/streamstat` so operators can pin
    /// runaway producers.
    std::string diagnosticsTag;
};

// ─── Result ────────────────────────────────────────────────────────────────

/// Outcome of a streaming request.  Distinct codes so producers can
/// branch on recoverable failures (retry later) versus permanent
/// ones (asset missing, asset kind mismatch).
enum class StreamingStatus : std::uint8_t
{
    Pending = 0,
    Ok,
    Cancelled,
    AssetNotFound,
    DecodeFailed,
    OutOfMemory,
    SourceError,
    ShuttingDown,
    InternalError,
};

/// Result payload published into the handle by the manager.  The
/// `bytes` vector holds the decoded asset in whatever shape the
/// consumer agreed to with the manager's `IAssetSource`.  It is
/// moved out of the handle on first consumption; subsequent reads
/// see an empty vector, which is the manager's way of protecting
/// against double-consumption bugs.
struct StreamingResultPayload
{
    AssetKind                 kind    = AssetKind::Unknown;
    std::uint8_t              lod     = 0;
    std::uint64_t             sizeBytes = 0;
    std::vector<std::uint8_t> bytes;
};

// ─── Handle ────────────────────────────────────────────────────────────────

/// Future-like handle returned by the streaming manager.  Shared
/// ownership so multiple consumers can wait on the same request
/// without duplicating the work.
class StreamingRequestHandle
{
public:
    StreamingRequestHandle() = default;

    [[nodiscard]] bool IsReady() const noexcept
    {
        return status_.load(std::memory_order_acquire) != StreamingStatus::Pending;
    }
    [[nodiscard]] StreamingStatus Status() const noexcept
    {
        return status_.load(std::memory_order_acquire);
    }
    [[nodiscard]] const StreamingResultPayload& Payload() const noexcept { return payload_; }

    /// Cooperative cancellation.  Requests that have not yet begun
    /// I/O are dropped; requests already decoding finish and then
    /// discard the result.  Idempotent.
    void Cancel() noexcept { cancelRequested_.store(true, std::memory_order_release); }
    [[nodiscard]] bool CancelRequested() const noexcept
    {
        return cancelRequested_.load(std::memory_order_acquire);
    }

    // ── Internal ────────────────────────────────────────────────────────
    // Called by the streaming manager to publish the final state.
    void SetPayload(StreamingResultPayload&& payload) noexcept
    {
        payload_ = std::move(payload);
        status_.store(StreamingStatus::Ok, std::memory_order_release);
    }
    void SetStatus(StreamingStatus status) noexcept
    {
        status_.store(status, std::memory_order_release);
    }

private:
    std::atomic<StreamingStatus> status_{StreamingStatus::Pending};
    std::atomic<bool>            cancelRequested_{false};
    StreamingResultPayload       payload_;
};

using StreamingRequestHandlePtr = std::shared_ptr<StreamingRequestHandle>;

/// Completion callback.  Optional — callers can poll the handle
/// instead.  The callback runs on the streaming manager's dispatch
/// thread; handlers are expected to be fast and to push heavy work
/// onto the caller's own queue.
using StreamingCompletionCallback = std::function<void(const StreamingRequestHandlePtr&)>;

// ─── Limits ────────────────────────────────────────────────────────────────

/// Maximum bytes in a single streamed payload.  Above this the
/// manager refuses the request — assets larger than this need to be
/// split into chunks (mesh LODs, texture mips) or restructured.
inline constexpr std::uint64_t kMaxStreamingPayloadBytes = 256ull * 1024 * 1024;

/// Hard ceiling on the outstanding request queue.  Protects the
/// manager from a runaway producer that floods the queue faster
/// than the I/O subsystem can drain it; excess requests are
/// rejected with `SourceError` so the producer learns to back off.
inline constexpr std::uint32_t kMaxStreamingQueueDepth = 4096;

} // namespace SagaEngine::Resources
