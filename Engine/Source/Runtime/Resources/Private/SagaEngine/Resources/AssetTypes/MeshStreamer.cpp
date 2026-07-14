/// @file MeshStreamer.cpp
/// @brief Typed streaming wrapper for mesh assets.

#include "SagaEngine/Resources/AssetTypes/MeshStreamer.h"
#include "SagaEngine/Resources/Formats/GltfMeshImporter.h"

#include "SagaEngine/Core/Log/Log.h"

#include <optional>
#include <utility>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

MeshStreamer::MeshStreamer(ResourceManager&   resourceManager,
                           MeshStreamerConfig config) noexcept
    : resourceManager_(resourceManager)
    , config_(config)
{
    LOG_INFO(kLogTag,
             "MeshStreamer: decoded cache budget=%llu bytes",
             static_cast<unsigned long long>(config_.decodedCacheBudget));
}

// ─── Acquire ───────────────────────────────────────────────────────────────

std::optional<Render::MeshAsset> MeshStreamer::Acquire(
    AssetId            meshId,
    StreamingPriority  priority)
{
    // Check the decoded cache first.
    {
        std::lock_guard lock(cacheMutex_);
        auto it = decodedCache_.find(meshId);
        if (it != decodedCache_.end())
        {
            TouchLruLocked(meshId);
            return it->second;
        }
    }

    // Check the resource manager for raw bytes.
    auto handle = resourceManager_.Acquire(meshId, AssetKind::Mesh, priority);

    if (!handle.IsValid() || !handle.IsReady())
        return std::nullopt;

    if (handle.Status() != StreamingStatus::Ok)
    {
        LOG_WARN(kLogTag,
                 "MeshStreamer: failed to stream mesh assetId=%llu status=%u",
                 static_cast<unsigned long long>(meshId),
                 static_cast<unsigned>(handle.Status()));
        return std::nullopt;
    }

    // Decode the bytes.
    std::vector<std::uint8_t> bytes(handle.Data(), handle.Data() + handle.Size());
    Render::MeshAsset mesh;

    if (!DecodeMesh(bytes, "mesh_" + std::to_string(meshId), meshId, mesh))
    {
        LOG_WARN(kLogTag,
                 "MeshStreamer: failed to decode mesh assetId=%llu",
                 static_cast<unsigned long long>(meshId));
        return std::nullopt;
    }

    // Cache the decoded mesh.
    {
        std::lock_guard lock(cacheMutex_);

        // Check again in case another thread decoded it while we were
        // waiting for the import.
        auto it = decodedCache_.find(meshId);
        if (it != decodedCache_.end())
        {
            TouchLruLocked(meshId);
            return it->second;
        }

        // Estimate decoded size.
        std::uint64_t meshBytes = 0;
        for (std::uint8_t lodIdx = 0; lodIdx < mesh.lodCount; ++lodIdx)
        {
            const auto& lod = mesh.lods[lodIdx];
            meshBytes += lod.vertices.size() * sizeof(Render::MeshVertex);
            meshBytes += lod.indices.size() * sizeof(std::uint32_t);
        }

        decodedCache_[meshId] = mesh;
        lruList_.push_back(meshId);
        lruPositions_[meshId] = std::prev(lruList_.end());
        decodedCacheBytes_ += meshBytes;

        EvictToBudgetLocked();

        return mesh;
    }
}

// ─── IsReady ───────────────────────────────────────────────────────────────

bool MeshStreamer::IsReady(AssetId meshId) const noexcept
{
    // Check decoded cache.
    {
        std::lock_guard lock(cacheMutex_);
        if (decodedCache_.count(meshId) > 0)
            return true;
    }

    // Check resource manager — we can't easily peek without acquiring,
    // so we just say "not ready" if it's not in the decoded cache.
    // A more sophisticated implementation would expose a `Peek` method
    // on the resource manager.
    return false;
}

// ─── Prefetch ──────────────────────────────────────────────────────────────

void MeshStreamer::Prefetch(AssetId meshId, StreamingPriority priority)
{
    // Submit a low-priority streaming request.  The completion callback
    // will decode the mesh and populate the cache asynchronously.
    auto handle = resourceManager_.Acquire(meshId, AssetKind::Mesh, priority);

    // If already ready, decode now.
    if (handle.IsValid() && handle.IsReady() && handle.Status() == StreamingStatus::Ok)
    {
        std::vector<std::uint8_t> bytes(handle.Data(), handle.Data() + handle.Size());
        Render::MeshAsset mesh;
        if (DecodeMesh(bytes, "mesh_" + std::to_string(meshId), meshId, mesh))
        {
            std::lock_guard lock(cacheMutex_);
            if (decodedCache_.find(meshId) == decodedCache_.end())
            {
                std::uint64_t meshBytes = 0;
                for (std::uint8_t lodIdx = 0; lodIdx < mesh.lodCount; ++lodIdx)
                {
                    const auto& lod = mesh.lods[lodIdx];
                    meshBytes += lod.vertices.size() * sizeof(Render::MeshVertex);
                    meshBytes += lod.indices.size() * sizeof(std::uint32_t);
                }

                decodedCache_[meshId] = std::move(mesh);
                lruList_.push_back(meshId);
                lruPositions_[meshId] = std::prev(lruList_.end());
                decodedCacheBytes_ += meshBytes;
                EvictToBudgetLocked();
            }
        }
    }
}

// ─── DecodeMesh ────────────────────────────────────────────────────────────

bool MeshStreamer::DecodeMesh(const std::vector<std::uint8_t>& bytes,
                              const std::string&               sourcePath,
                              AssetId                          meshId,
                              Render::MeshAsset&               out)
{
    if (bytes.empty())
        return false;

    auto result = GltfMeshImporter::ImportFromBytes(bytes, sourcePath, meshId);
    if (!result.success)
    {
        LOG_WARN(kLogTag,
                 "MeshStreamer: glTF import failed for meshId=%llu: %s",
                 static_cast<unsigned long long>(meshId),
                 result.diagnostic.c_str());
        return false;
    }

    out = std::move(result.mesh);
    return true;
}

// ─── CachedMeshCount ───────────────────────────────────────────────────────

std::size_t MeshStreamer::CachedMeshCount() const noexcept
{
    std::lock_guard lock(cacheMutex_);
    return decodedCache_.size();
}

// ─── CachedMeshBytes ───────────────────────────────────────────────────────

std::uint64_t MeshStreamer::CachedMeshBytes() const noexcept
{
    std::lock_guard lock(cacheMutex_);
    return decodedCacheBytes_;
}

// ─── EvictToBudgetLocked ───────────────────────────────────────────────────

void MeshStreamer::TouchLruLocked(AssetId meshId)
{
    auto posIt = lruPositions_.find(meshId);
    if (posIt == lruPositions_.end())
        return;

    // Move the entry to the back of the LRU list (most-recently-used).
    lruList_.splice(lruList_.end(), lruList_, posIt->second);
}

void MeshStreamer::EvictToBudgetLocked()
{
    if (config_.decodedCacheBudget == 0)
        return;

    // Evict from the front of the LRU list (least-recently-used)
    // until we fit the budget.
    while (decodedCacheBytes_ > config_.decodedCacheBudget && !lruList_.empty())
    {
        AssetId victimId = lruList_.front();
        lruList_.pop_front();
        lruPositions_.erase(victimId);

        auto it = decodedCache_.find(victimId);
        if (it == decodedCache_.end())
            continue;

        std::uint64_t freed = 0;
        const auto& mesh = it->second;
        for (std::uint8_t lodIdx = 0; lodIdx < mesh.lodCount; ++lodIdx)
        {
            const auto& lod = mesh.lods[lodIdx];
            freed += lod.vertices.size() * sizeof(Render::MeshVertex);
            freed += lod.indices.size() * sizeof(std::uint32_t);
        }

        decodedCache_.erase(it);

        if (freed > decodedCacheBytes_)
            decodedCacheBytes_ = 0;
        else
            decodedCacheBytes_ -= freed;
    }
}

} // namespace SagaEngine::Resources
