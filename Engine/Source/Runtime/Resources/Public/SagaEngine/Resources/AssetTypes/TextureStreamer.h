/// @file TextureStreamer.h
/// @brief Typed streaming wrapper for texture assets.
///
/// Layer  : SagaEngine / Resources / AssetTypes
/// Purpose: `ResourceManager` hands back opaque bytes.  Consumers that
///          want a typed `TextureAsset` need to decode those bytes
///          (DDS, KTX2).  `TextureStreamer` is the convenience layer
///          that combines the resource manager, the `TextureImporter`,
///          and a decoded cache so callers ask for a texture by id
///          and get a `Resources::TextureAsset` without worrying
///          about the import pipeline.
///
///          This is the public entry point for texture streaming in
///          an MMO client: the renderer asks for the textures in the
///          new zone's apron, the `TextureStreamer` checks the
///          decoded cache, falls back to the resource manager on a
///          miss, runs the importer, and hands the caller a
///          ready-to-upload `TextureAsset`.
///
/// Design rules:
///   - One texture at a time.  The streamer does not batch imports;
///     each `Acquire` triggers one streaming request and one import.
///     Batching is the zone loader's job.
///   - Thread-safe.  The decoded cache is guarded by a mutex so
///     several worker threads can request textures concurrently.
///   - Budgeted.  The decoded cache has its own byte ceiling so
///     decoded textures do not starve mesh residency.
///
/// What this header is NOT:
///   - Not a texture transcoder.  BC3 stays BC3; ASTC stays ASTC.
///     Transcoding is a cook-time operation.
///   - Not a texture atlas manager.  Packing multiple textures into
///     one atlas is a separate toolchain step.

#pragma once

#include "SagaEngine/Resources/Formats/TextureImporter.h"
#include "SagaEngine/Resources/ResourceManager.h"

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Resources {

// ─── Config ────────────────────────────────────────────────────────────────

/// Constructor parameters for `TextureStreamer`.
struct TextureStreamerConfig
{
    /// Decoded texture cache budget in bytes.  Zero disables the cache
    /// (useful in tests that want to exercise the streaming path
    /// without residency side effects).
    std::uint64_t decodedCacheBudget = 256ull * 1024 * 1024; // 256 MiB.
};

// ─── TextureStreamer ───────────────────────────────────────────────────────

/// Typed texture streaming facade.  Sits on top of `ResourceManager`
/// and `TextureImporter`; callers interact with this instead of the
/// lower-level bytes pipeline.
class TextureStreamer
{
public:
    /// Build a streamer backed by the given resource manager.  The
    /// streamer does not own the manager — the caller keeps the
    /// lifetime and must outlive the streamer.
    explicit TextureStreamer(ResourceManager&      resourceManager,
                             TextureStreamerConfig config = {}) noexcept;

    ~TextureStreamer() noexcept = default;

    TextureStreamer(const TextureStreamer&)            = delete;
    TextureStreamer& operator=(const TextureStreamer&) = delete;
    TextureStreamer(TextureStreamer&&)                 = delete;
    TextureStreamer& operator=(TextureStreamer&&)      = delete;

    // ── Acquisition ───────────────────────────────────────────────────────

    /// Request a texture by asset id.  If the texture is already in
    /// the decoded cache, a copy is returned immediately.  If not,
    /// the streamer checks the resource manager's residency map; if
    /// the raw bytes are resident, it decodes them and caches the
    /// result.  If the bytes are not resident either, it submits a
    /// streaming request and returns an empty optional — the caller
    /// should poll or await readiness.
    ///
    /// Threading: safe to call from any thread.
    [[nodiscard]] std::optional<TextureAsset> Acquire(
        AssetId            textureId,
        StreamingPriority  priority = StreamingPriority::Normal);

    /// Test whether a texture is ready (decoded and cached).  Used
    /// by callers that prefer polling over blocking.
    [[nodiscard]] bool IsReady(AssetId textureId) const noexcept;

    /// Force a texture into the decoded cache if it is not already
    /// present.  Used by the zone prefetcher to warm the cache
    /// before the player crosses a boundary.
    void Prefetch(AssetId textureId, StreamingPriority priority = StreamingPriority::Low);

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Number of decoded textures currently cached.
    [[nodiscard]] std::size_t CachedTextureCount() const noexcept;

    /// Total bytes consumed by the decoded texture cache.
    [[nodiscard]] std::uint64_t CachedTextureBytes() const noexcept;

private:
    /// Decode raw bytes into a `TextureAsset`.  Returns false if the
    /// importer fails (unknown format, corrupted payload).
    [[nodiscard]] bool DecodeTexture(const std::vector<std::uint8_t>& bytes,
                                     const std::string&               sourcePath,
                                     AssetId                          textureId,
                                     TextureAsset&                    out);

    ResourceManager&      resourceManager_;
    TextureStreamerConfig config_;

    // ─── Decoded cache ────────────────────────────────────────────────────
    // Guarded by `cacheMutex_`.  The cache stores fully decoded
    // textures so subsequent `Acquire` calls skip the import step.
    // LRU ordering is maintained via a doubly-linked list so
    // eviction removes the least-recently-used texture rather than
    // an arbitrary entry from the hash map.
    mutable std::mutex cacheMutex_;
    std::unordered_map<AssetId, TextureAsset> decodedCache_;
    std::list<AssetId>                        lruList_;
    std::unordered_map<AssetId, std::list<AssetId>::iterator> lruPositions_;
    std::uint64_t                            decodedCacheBytes_ = 0;

    /// Touch an entry so it moves to the back of the LRU list
    /// (most-recently-used position).  Must be called with
    /// `cacheMutex_` held.
    void TouchLruLocked(AssetId textureId);

    /// Evict decoded textures until the cache fits the budget.  Must
    /// be called with `cacheMutex_` held.
    void EvictToBudgetLocked();
};

} // namespace SagaEngine::Resources
