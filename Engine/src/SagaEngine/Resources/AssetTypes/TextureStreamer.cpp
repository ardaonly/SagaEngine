/// @file TextureStreamer.cpp
/// @brief Typed streaming wrapper for texture assets.

#include "SagaEngine/Resources/AssetTypes/TextureStreamer.h"
#include "SagaEngine/Resources/Formats/TextureImporter.h"

#include "SagaEngine/Core/Log/Log.h"

#include <optional>
#include <utility>

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

TextureStreamer::TextureStreamer(ResourceManager&    resourceManager,
                                 TextureStreamerConfig config) noexcept
    : resourceManager_(resourceManager)
    , config_(config)
{
    LOG_INFO(kLogTag,
             "TextureStreamer: decoded cache budget=%llu bytes",
             static_cast<unsigned long long>(config_.decodedCacheBudget));
}

// ─── Acquire ───────────────────────────────────────────────────────────────

std::optional<TextureAsset> TextureStreamer::Acquire(
    AssetId            textureId,
    StreamingPriority  priority)
{
    // Check the decoded cache first.
    {
        std::lock_guard lock(cacheMutex_);
        auto it = decodedCache_.find(textureId);
        if (it != decodedCache_.end())
        {
            TouchLruLocked(textureId);
            return it->second;
        }
    }

    // Check the resource manager for raw bytes.
    auto handle = resourceManager_.Acquire(textureId, AssetKind::Texture, priority);

    if (!handle.IsValid() || !handle.IsReady())
        return std::nullopt;

    if (handle.Status() != StreamingStatus::Ok)
    {
        LOG_WARN(kLogTag,
                 "TextureStreamer: failed to stream texture assetId=%llu status=%u",
                 static_cast<unsigned long long>(textureId),
                 static_cast<unsigned>(handle.Status()));
        return std::nullopt;
    }

    // Decode the bytes.
    std::vector<std::uint8_t> bytes(handle.Data(), handle.Data() + handle.Size());
    TextureAsset texture;

    if (!DecodeTexture(bytes, "texture_" + std::to_string(textureId), textureId, texture))
    {
        LOG_WARN(kLogTag,
                 "TextureStreamer: failed to decode texture assetId=%llu",
                 static_cast<unsigned long long>(textureId));
        return std::nullopt;
    }

    // Cache the decoded texture.
    {
        std::lock_guard lock(cacheMutex_);

        // Check again in case another thread decoded it while we were
        // waiting for the import.
        auto it = decodedCache_.find(textureId);
        if (it != decodedCache_.end())
        {
            TouchLruLocked(textureId);
            return it->second;
        }

        // Use the importer's GPU footprint estimate.
        std::uint64_t textureBytes = texture.approxGpuBytes;

        decodedCache_[textureId] = texture;
        lruList_.push_back(textureId);
        lruPositions_[textureId] = std::prev(lruList_.end());
        decodedCacheBytes_ += textureBytes;

        EvictToBudgetLocked();

        return texture;
    }
}

// ─── IsReady ───────────────────────────────────────────────────────────────

bool TextureStreamer::IsReady(AssetId textureId) const noexcept
{
    // Check decoded cache.
    {
        std::lock_guard lock(cacheMutex_);
        if (decodedCache_.count(textureId) > 0)
            return true;
    }

    // Check resource manager — we can't easily peek without acquiring,
    // so we just say "not ready" if it's not in the decoded cache.
    return false;
}

// ─── Prefetch ──────────────────────────────────────────────────────────────

void TextureStreamer::Prefetch(AssetId textureId, StreamingPriority priority)
{
    // Submit a low-priority streaming request.  The completion callback
    // will decode the texture and populate the cache asynchronously.
    auto handle = resourceManager_.Acquire(textureId, AssetKind::Texture, priority);

    // If already ready, decode now.
    if (handle.IsValid() && handle.IsReady() && handle.Status() == StreamingStatus::Ok)
    {
        std::vector<std::uint8_t> bytes(handle.Data(), handle.Data() + handle.Size());
        TextureAsset texture;
        if (DecodeTexture(bytes, "texture_" + std::to_string(textureId), textureId, texture))
        {
            std::lock_guard lock(cacheMutex_);
            if (decodedCache_.find(textureId) == decodedCache_.end())
            {
                decodedCache_[textureId] = std::move(texture);
                lruList_.push_back(textureId);
                lruPositions_[textureId] = std::prev(lruList_.end());
                decodedCacheBytes_ += texture.approxGpuBytes;
                EvictToBudgetLocked();
            }
        }
    }
}

// ─── DecodeTexture ─────────────────────────────────────────────────────────

bool TextureStreamer::DecodeTexture(const std::vector<std::uint8_t>& bytes,
                                    const std::string&               sourcePath,
                                    AssetId                          textureId,
                                    TextureAsset&                    out)
{
    if (bytes.empty())
        return false;

    auto result = TextureImporter::ImportFromBytes(bytes, sourcePath, textureId);
    if (!result.success)
    {
        LOG_WARN(kLogTag,
                 "TextureStreamer: texture import failed for textureId=%llu: %s",
                 static_cast<unsigned long long>(textureId),
                 result.diagnostic.c_str());
        return false;
    }

    out = std::move(result.texture);
    return true;
}

// ─── CachedTextureCount ────────────────────────────────────────────────────

std::size_t TextureStreamer::CachedTextureCount() const noexcept
{
    std::lock_guard lock(cacheMutex_);
    return decodedCache_.size();
}

// ─── CachedTextureBytes ────────────────────────────────────────────────────

std::uint64_t TextureStreamer::CachedTextureBytes() const noexcept
{
    std::lock_guard lock(cacheMutex_);
    return decodedCacheBytes_;
}

// ─── EvictToBudgetLocked ───────────────────────────────────────────────────

void TextureStreamer::TouchLruLocked(AssetId textureId)
{
    auto posIt = lruPositions_.find(textureId);
    if (posIt == lruPositions_.end())
        return;

    // Move the entry to the back of the LRU list (most-recently-used).
    lruList_.splice(lruList_.end(), lruList_, posIt->second);
}

void TextureStreamer::EvictToBudgetLocked()
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

        std::uint64_t freed = it->second.approxGpuBytes;

        decodedCache_.erase(it);

        if (freed > decodedCacheBytes_)
            decodedCacheBytes_ = 0;
        else
            decodedCacheBytes_ -= freed;
    }
}

} // namespace SagaEngine::Resources
