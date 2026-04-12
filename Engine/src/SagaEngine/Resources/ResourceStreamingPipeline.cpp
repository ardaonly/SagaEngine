/// @file ResourceStreamingPipeline.cpp
/// @brief Async resource streaming pipeline implementation.

#include "SagaEngine/Resources/ResourceStreamingPipeline.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Core/Profiling/Profiler.h"

#include <algorithm>
#include <chrono>

namespace SagaEngine::Resources {

static constexpr const char* kTag = "ResourceStreaming";

// ─── Resource type names ──────────────────────────────────────────────────────

const char* ResourceTypeToString(ResourceType type) noexcept
{
    switch (type)
    {
    case ResourceType::Mesh:      return "Mesh";
    case ResourceType::Texture:   return "Texture";
    case ResourceType::Material:  return "Material";
    case ResourceType::Audio:     return "Audio";
    case ResourceType::Animation: return "Animation";
    case ResourceType::Config:    return "Config";
    default:                      return "Unknown";
    }
}

// ─── Construction ─────────────────────────────────────────────────────────────

ResourceStreamingPipeline::ResourceStreamingPipeline() = default;

ResourceStreamingPipeline::~ResourceStreamingPipeline()
{
    Shutdown();
}

void ResourceStreamingPipeline::Configure(StreamingPipelineConfig config) noexcept
{
    config_ = config;
    LOG_INFO(kTag, "Streaming pipeline configured: workers=%u, budget=%zuMB",
             config.workerThreadCount, config.memoryBudgetBytes / (1024 * 1024));
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void ResourceStreamingPipeline::Start()
{
    if (running_.load())
    {
        LOG_WARN(kTag, "Pipeline already running.");
        return;
    }

    running_.store(true);
    activeWorkers_.store(0);

    const std::uint32_t threadCount = std::max(1u, config_.workerThreadCount);

    for (std::uint32_t i = 0; i < threadCount; ++i)
    {
        workerThreads_.emplace_back(&ResourceStreamingPipeline::WorkerThreadFunc, this);
    }

    LOG_INFO(kTag, "Streaming pipeline started with %u workers.", threadCount);
}

void ResourceStreamingPipeline::Shutdown()
{
    if (!running_.load())
        return;

    running_.store(false);
    requestCv_.notify_all();

    for (auto& thread : workerThreads_)
    {
        if (thread.joinable())
            thread.join();
    }

    workerThreads_.clear();

    // Release all loaded resources
    {
        std::unique_lock<std::shared_mutex> lock(resourceMutex_);
        for (auto& [path, resource] : loadedResources_)
        {
            if (resource.data && releaseFn_)
                releaseFn_(resource.data, resource.memoryBytes);
        }
        loadedResources_.clear();
        currentMemoryUsage_ = 0;
    }

    LOG_INFO(kTag, "Streaming pipeline shut down. All resources released.");
}

// ─── Request submission ───────────────────────────────────────────────────────

std::uint64_t ResourceStreamingPipeline::SubmitRequest(StreamRequest request)
{
    const std::uint64_t requestId = nextRequestId_.fetch_add(1);
    request.requestId = requestId;

    {
        std::lock_guard<std::mutex> lock(requestMutex_);

        // Check if already loaded
        {
            std::shared_lock<std::shared_mutex> rLock(resourceMutex_);
            if (loadedResources_.count(request.assetPath) > 0)
            {
                // Already loaded — just update LRU tick
                auto& res = loadedResources_[request.assetPath];
                res.lastUsedTick = request.submitTick;
                return requestId;
            }
        }

        // Check if already in queue
        // (Linear scan — queue size is bounded by maxPendingRequests)
        // For simplicity, we allow duplicates; deduplication happens in worker.

        requestQueue_.push(std::move(request));
    }

    requestCv_.notify_one();
    return requestId;
}

void ResourceStreamingPipeline::CancelRequest(std::uint64_t requestId)
{
    // Cancellation is tricky with a priority_queue — we mark it as cancelled
    // by checking in the worker. For a full implementation, a separate
    // cancellation set would be needed.
    (void)requestId;
}

// ─── Resource access ──────────────────────────────────────────────────────────

const LoadedResource* ResourceStreamingPipeline::GetResource(
    const std::string& assetPath) const
{
    std::shared_lock<std::shared_mutex> lock(resourceMutex_);
    auto it = loadedResources_.find(assetPath);
    if (it != loadedResources_.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool ResourceStreamingPipeline::IsResourceLoaded(const std::string& assetPath) const
{
    std::shared_lock<std::shared_mutex> lock(resourceMutex_);
    return loadedResources_.count(assetPath) > 0;
}

void ResourceStreamingPipeline::SetResourceLodTarget(
    const std::string& assetPath, std::uint32_t targetLod)
{
    std::unique_lock<std::shared_mutex> lock(resourceMutex_);
    auto it = loadedResources_.find(assetPath);
    if (it != loadedResources_.end())
    {
        it->second.targetLod = targetLod;
        if (it->second.currentLod != targetLod)
        {
            it->second.lodTransitionProgress = 0.f;
        }
    }
}

// ─── Per-frame updates ────────────────────────────────────────────────────────

void ResourceStreamingPipeline::Tick(std::uint64_t currentTick, float deltaTimeSec)
{
    SAGA_PROFILE_SCOPE("ResourceStreamingPipeline::Tick");

    // Process completion callbacks (main thread only)
    {
        std::lock_guard<std::mutex> lock(completionMutex_);
        for (const auto& entry : completedRequests_)
        {
            if (entry.success)
            {
                LoadedResource resource;
                resource.assetPath = entry.assetPath;
                resource.memoryBytes = entry.memoryBytes;
                resource.lastUsedTick = currentTick;
                resource.loadedTick = currentTick;
                resource.data = entry.data;
                resource.currentLod = 0;
                resource.targetLod = 0;
                resource.lodTransitionProgress = 1.f;

                {
                    std::unique_lock<std::shared_mutex> rLock(resourceMutex_);
                    loadedResources_[entry.assetPath] = std::move(resource);
                    currentMemoryUsage_ += entry.memoryBytes;
                }

                if (onLoaded_)
                    onLoaded_(entry.requestId, entry.assetPath, entry.data);
            }
            else
            {
                if (onFailed_)
                    onFailed_(entry.requestId, entry.assetPath);
            }
        }
        completedRequests_.clear();
    }

    // Update LOD transitions
    UpdateLodTransitions(deltaTimeSec, currentTick);

    // Enforce memory budget periodically
    if (currentTick - lastBudgetEnforcementTick_ >= config_.budgetEnforcementTickInterval)
    {
        EnforceMemoryBudget(currentTick);
        lastBudgetEnforcementTick_ = currentTick;
    }
}

// ─── Diagnostics ──────────────────────────────────────────────────────────────

std::size_t ResourceStreamingPipeline::GetPendingRequestCount() const noexcept
{
    std::lock_guard<std::mutex> lock(requestMutex_);
    return requestQueue_.size();
}

std::size_t ResourceStreamingPipeline::GetLoadedResourceCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(resourceMutex_);
    return loadedResources_.size();
}

std::size_t ResourceStreamingPipeline::GetCurrentMemoryUsage() const noexcept
{
    std::shared_lock<std::shared_mutex> lock(resourceMutex_);
    return currentMemoryUsage_;
}

std::uint32_t ResourceStreamingPipeline::GetActiveWorkerCount() const noexcept
{
    return activeWorkers_.load();
}

// ─── Worker thread ────────────────────────────────────────────────────────────

void ResourceStreamingPipeline::WorkerThreadFunc()
{
    activeWorkers_.fetch_add(1);

    while (running_.load())
    {
        StreamRequest request;

        {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait_for(lock, std::chrono::milliseconds(10), [this]()
            {
                return !requestQueue_.empty() || !running_.load();
            });

            if (!running_.load())
                break;

            if (requestQueue_.empty())
                continue;

            request = requestQueue_.top();
            requestQueue_.pop();
        }

        SAGA_PROFILE_SCOPE("StreamWorker::LoadResource");

        // Check if already loaded (double-check after dequeue)
        {
            std::shared_lock<std::shared_mutex> rLock(resourceMutex_);
            if (loadedResources_.count(request.assetPath) > 0)
            {
                continue; // Skip — already loaded
            }
        }

        // Load the resource
        if (loadFn_)
        {
            std::size_t memoryBytes = 0;
            void* data = loadFn_(request.assetPath, memoryBytes);

            CompletionEntry entry;
            entry.requestId = request.requestId;
            entry.assetPath = request.assetPath;
            entry.data = data;
            entry.memoryBytes = memoryBytes;
            entry.success = (data != nullptr);

            {
                std::lock_guard<std::mutex> lock(completionMutex_);
                completedRequests_.push_back(std::move(entry));
            }
        }
        else
        {
            // No load function — simulate success for testing
            CompletionEntry entry;
            entry.requestId = request.requestId;
            entry.assetPath = request.assetPath;
            entry.data = nullptr; // Placeholder
            entry.memoryBytes = 1024; // Simulated 1KB
            entry.success = true;

            {
                std::lock_guard<std::mutex> lock(completionMutex_);
                completedRequests_.push_back(std::move(entry));
            }
        }
    }

    activeWorkers_.fetch_sub(1);
}

// ─── Memory management ────────────────────────────────────────────────────────

void ResourceStreamingPipeline::EnforceMemoryBudget(std::uint64_t currentTick)
{
    SAGA_PROFILE_SCOPE("ResourceStreamingPipeline::EnforceMemoryBudget");

    while (currentMemoryUsage_ > config_.memoryBudgetBytes)
    {
        if (!EvictLruResource())
            break; // Nothing left to evict
    }

    (void)currentTick;
}

bool ResourceStreamingPipeline::EvictLruResource()
{
    std::unique_lock<std::shared_mutex> lock(resourceMutex_);

    if (loadedResources_.empty())
        return false;

    // Find the least recently used resource
    auto lruIt = loadedResources_.end();
    std::uint64_t oldestTick = UINT64_MAX;

    for (auto it = loadedResources_.begin(); it != loadedResources_.end(); ++it)
    {
        if (it->second.lastUsedTick < oldestTick)
        {
            oldestTick = it->second.lastUsedTick;
            lruIt = it;
        }
    }

    if (lruIt == loadedResources_.end())
        return false;

    const std::string path = lruIt->first;
    const std::size_t memoryBytes = lruIt->second.memoryBytes;
    void* data = lruIt->second.data;

    // Release the resource
    if (data && releaseFn_)
    {
        releaseFn_(data, memoryBytes);
    }

    currentMemoryUsage_ -= memoryBytes;
    loadedResources_.erase(lruIt);

    if (onEvicted_)
        onEvicted_(path);

    return true;
}

// ─── LOD transitions ──────────────────────────────────────────────────────────

void ResourceStreamingPipeline::UpdateLodTransitions(float deltaTimeSec,
                                                      std::uint64_t currentTick)
{
    SAGA_PROFILE_SCOPE("ResourceStreamingPipeline::UpdateLodTransitions");

    std::unique_lock<std::shared_mutex> lock(resourceMutex_);

    for (auto& [path, resource] : loadedResources_)
    {
        if (resource.currentLod != resource.targetLod &&
            resource.lodTransitionProgress < 1.f)
        {
            resource.lodTransitionProgress +=
                deltaTimeSec / config_.lodTransitionDurationSec;

            if (resource.lodTransitionProgress >= 1.f)
            {
                resource.lodTransitionProgress = 1.f;
                resource.currentLod = resource.targetLod;
            }
        }

        resource.lastUsedTick = currentTick;
    }
}

} // namespace SagaEngine::Resources
