/// @file MeshStreamer.h
/// @brief Typed streaming wrapper for mesh assets.
///
/// Layer  : SagaEngine / Resources / AssetTypes
/// Purpose: `ResourceManager` hands back opaque bytes.  Consumers that
///          want a typed `MeshAsset` need to decode those bytes
///          (glTF, FBX, custom binary).  `MeshStreamer` is the
///          convenience layer that combines the resource manager,
///          the `GltfMeshImporter`, and a decoded cache so callers
///          ask for a mesh by id and get a `Render::MeshAsset`
///          without worrying about the import pipeline.
///
///          This is the public entry point for mesh streaming in an
///          MMO client: the zone loader asks for the meshes in the
///          new zone's apron, the `MeshStreamer` checks the decoded
///          cache, falls back to the resource manager on a miss,
///          runs the importer, and hands the caller a ready-to-use
///          `MeshAsset`.
///
/// Design rules:
///   - One mesh at a time.  The streamer does not batch imports;
///     each `Acquire` triggers one streaming request and one
///     import.  Batching is the zone loader's job.
///   - Thread-safe.  The decoded cache is guarded by a mutex so
///     several worker threads can request meshes concurrently.
///   - Budgeted.  The decoded cache has its own byte ceiling so
///     decoded meshes do not starve texture residency.
///
/// What this header is NOT:
///   - Not a mesh optimiser.  LOD generation, meshopt simplification,
///     and vertex cache optimisation are cook-time operations.
///   - Not a skeletal animation player.  Skin data rides a sibling
///     header once skeletal animation lands.

#pragma once

#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Resources/ResourceManager.h"

#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace SagaEngine::Resources {

// ─── Config ────────────────────────────────────────────────────────────────

/// Constructor parameters for `MeshStreamer`.
struct MeshStreamerConfig
{
    /// Decoded mesh cache budget in bytes.  Zero disables the cache
    /// (useful in tests that want to exercise the streaming path
    /// without residency side effects).
    std::uint64_t decodedCacheBudget = 128ull * 1024 * 1024; // 128 MiB.
};

// ─── MeshStreamer ──────────────────────────────────────────────────────────

/// Typed mesh streaming facade.  Sits on top of `ResourceManager` and
/// `GltfMeshImporter`; callers interact with this instead of the
/// lower-level bytes pipeline.
class MeshStreamer
{
public:
    /// Build a streamer backed by the given resource manager.  The
    /// streamer does not own the manager — the caller keeps the
    /// lifetime and must outlive the streamer.
    explicit MeshStreamer(ResourceManager&       resourceManager,
                          MeshStreamerConfig     config = {}) noexcept;

    ~MeshStreamer() noexcept = default;

    MeshStreamer(const MeshStreamer&)            = delete;
    MeshStreamer& operator=(const MeshStreamer&) = delete;
    MeshStreamer(MeshStreamer&&)                 = delete;
    MeshStreamer& operator=(MeshStreamer&&)      = delete;

    // ── Acquisition ───────────────────────────────────────────────────────

    /// Request a mesh by asset id.  If the mesh is already in the
    /// decoded cache, a copy is returned immediately.  If not, the
    /// streamer checks the resource manager's residency map; if the
    /// raw bytes are resident, it decodes them and caches the
    /// result.  If the bytes are not resident either, it submits a
    /// streaming request and returns an empty optional — the caller
    /// should poll or await readiness.
    ///
    /// Threading: safe to call from any thread.
    [[nodiscard]] std::optional<Render::MeshAsset> Acquire(
        AssetId            meshId,
        StreamingPriority  priority = StreamingPriority::Normal);

    /// Test whether a mesh is ready (decoded and cached).  Used by
    /// callers that prefer polling over blocking.
    [[nodiscard]] bool IsReady(AssetId meshId) const noexcept;

    /// Force a mesh into the decoded cache if it is not already
    /// present.  Used by the zone prefetcher to warm the cache
    /// before the player crosses a boundary.
    void Prefetch(AssetId meshId, StreamingPriority priority = StreamingPriority::Low);

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Number of decoded meshes currently cached.
    [[nodiscard]] std::size_t CachedMeshCount() const noexcept;

    /// Total bytes consumed by the decoded mesh cache.
    [[nodiscard]] std::uint64_t CachedMeshBytes() const noexcept;

private:
    /// Decode raw bytes into a `MeshAsset`.  Returns false if the
    /// importer fails (unknown format, corrupted payload).
    [[nodiscard]] bool DecodeMesh(const std::vector<std::uint8_t>& bytes,
                                  const std::string&               sourcePath,
                                  AssetId                          meshId,
                                  Render::MeshAsset&               out);

    ResourceManager&       resourceManager_;
    MeshStreamerConfig     config_;

    // ─── Decoded cache ────────────────────────────────────────────────────
    // Guarded by `cacheMutex_`.  The cache stores fully decoded
    // meshes so subsequent `Acquire` calls skip the import step.
    // LRU ordering is maintained via a doubly-linked list so
    // eviction removes the least-recently-used mesh rather than
    // an arbitrary entry from the hash map.
    mutable std::mutex cacheMutex_;
    std::unordered_map<AssetId, Render::MeshAsset> decodedCache_;
    std::list<AssetId>                             lruList_;
    std::unordered_map<AssetId, std::list<AssetId>::iterator> lruPositions_;
    std::uint64_t                                  decodedCacheBytes_ = 0;

    /// Touch an entry so it moves to the back of the LRU list
    /// (most-recently-used position).  Must be called with
    /// `cacheMutex_` held.
    void TouchLruLocked(AssetId meshId);

    /// Evict decoded meshes until the cache fits the budget.  Must
    /// be called with `cacheMutex_` held.
    void EvictToBudgetLocked();
};

} // namespace SagaEngine::Resources
