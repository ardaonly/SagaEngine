/// @file ResourceStreamingPipeline.h
/// @brief Async resource streaming pipeline with priority queue, memory budget, and LOD smoothing.
///
/// Layer  : SagaEngine / Resources / Streaming
/// Purpose: Manages asynchronous loading of game resources (meshes, textures,
///          audio, etc.) with intelligent prioritization, memory budget
///          enforcement, and smooth LOD transitions to avoid popping.
///
/// Design:
///   - Worker threads process a priority queue of streaming requests.
///   - Requests are prioritized by distance to player, visibility, and type.
///   - Memory budget is enforced by evicting least-recently-used resources.
///   - LOD transitions are smoothed over time to avoid visual popping.
///
/// Thread safety: Request queue is thread-safe; loaded resource map is
///                protected by a shared mutex for concurrent reads.

#pragma once

#include "SagaEngine/Core/Threading/JobSystem.h"
#include "SagaEngine/Core/Memory/ArenaAllocator.h"
#include "SagaEngine/Core/Threading/SpinLock.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Resources {

// ─── Resource types ─────────────────────────────────────────────────────────

enum class ResourceType : std::uint8_t
{
    Mesh,
    Texture,
    Material,
    Audio,
    Animation,
    Config,
    Count
};

/// Human-readable resource type name.
[[nodiscard]] const char* ResourceTypeToString(ResourceType type) noexcept;

// ─── Streaming priority ─────────────────────────────────────────────────────

/// Priority level for streaming requests.
enum class StreamPriority : std::uint8_t
{
    Critical = 0,   ///< Must load immediately (e.g. player character, current zone).
    High     = 1,   ///< Load ASAP (e.g. nearby entities, visible objects).
    Normal   = 2,   ///< Standard priority (e.g. background elements).
    Low      = 3,   ///< Load when idle (e.g. distant decorations).
    Prefetch = 4,   ///< Speculative loading (e.g. predicted path).
    Count
};

// ─── Streaming request ──────────────────────────────────────────────────────

/// A single resource streaming request.
struct StreamRequest
{
    std::string         assetPath;          ///< Path to the asset file.
    ResourceType        type = ResourceType::Mesh;
    StreamPriority      priority = StreamPriority::Normal;
    std::uint64_t       requestId = 0;      ///< Unique request ID.
    float               distanceToPlayer = 0.f; ///< For priority sorting.
    std::uint64_t       submitTick = 0;     ///< Tick when request was submitted.

    /// Comparator for priority queue (lower priority value = higher priority).
    bool operator<(const StreamRequest& other) const noexcept
    {
        if (priority != other.priority)
            return static_cast<std::uint8_t>(priority) >
                   static_cast<std::uint8_t>(other.priority);
        // Within same priority, closer entities first
        return distanceToPlayer > other.distanceToPlayer;
    }
};

// ─── Loaded resource entry ──────────────────────────────────────────────────

/// Metadata for a loaded resource.
struct LoadedResource
{
    std::string         assetPath;
    ResourceType        type = ResourceType::Mesh;
    std::size_t         memoryBytes = 0;    ///< Memory footprint.
    std::uint64_t       lastUsedTick = 0;   ///< Last access tick (for LRU eviction).
    std::uint64_t       loadedTick = 0;     ///< Tick when resource was loaded.
    void*               data = nullptr;     ///< Opaque pointer to loaded data.
    std::uint32_t       currentLod = 0;     ///< Current LOD level.
    std::uint32_t       targetLod = 0;      ///< Target LOD level.
    float               lodTransitionProgress = 1.f; ///< 0.0 = start, 1.0 = complete.
};

// ─── Streaming pipeline config ──────────────────────────────────────────────

struct StreamingPipelineConfig
{
    /// Number of async worker threads for loading.
    std::uint32_t workerThreadCount = 2;

    /// Maximum memory budget for loaded resources (default 512 MB).
    std::size_t memoryBudgetBytes = 512u * 1024u * 1024u;

    /// Maximum number of pending requests in the queue.
    std::uint32_t maxPendingRequests = 1024;

    /// LOD transition duration in seconds (default 0.5s).
    float lodTransitionDurationSec = 0.5f;

    /// Tick interval for memory budget enforcement (default 60 ticks).
    std::uint32_t budgetEnforcementTickInterval = 60;
};

// ─── Streaming callbacks ────────────────────────────────────────────────────

/// Called when a resource finishes loading.
using OnResourceLoadedFn = std::function<void(
    std::uint64_t requestId,
    const std::string& assetPath,
    void* data)>;

/// Called when a resource fails to load.
using OnResourceLoadFailedFn = std::function<void(
    std::uint64_t requestId,
    const std::string& assetPath)>;

/// Called when a resource is evicted from memory.
using OnResourceEvictedFn = std::function<void(
    const std::string& assetPath)>;

/// Actual loading function (provided by the asset system).
using LoadResourceFn = std::function<void*(
    const std::string& assetPath,
    std::size_t& outMemoryBytes)>;

/// Release resource memory (provided by the asset system).
using ReleaseResourceFn = std::function<void(
    void* data,
    std::size_t memoryBytes)>;

// ─── Streaming Pipeline ─────────────────────────────────────────────────────

/// Async resource streaming pipeline with priority queue and memory management.
///
/// Thread-safe: requests can be submitted from any thread; workers process
/// on background threads; completion callbacks fire on the main thread.
class ResourceStreamingPipeline
{
public:
    ResourceStreamingPipeline();
    ~ResourceStreamingPipeline();

    ResourceStreamingPipeline(const ResourceStreamingPipeline&)            = delete;
    ResourceStreamingPipeline& operator=(const ResourceStreamingPipeline&) = delete;

    /// Configure the pipeline. Must be called before Start().
    void Configure(StreamingPipelineConfig config) noexcept;

    /// Start worker threads. Call from main thread during init.
    void Start();

    /// Stop all workers and flush pending requests. Call during shutdown.
    void Shutdown();

    // ─── Request submission ───────────────────────────────────────────────────

    /// Submit a streaming request. Thread-safe.
    /// Returns the request ID for tracking.
    [[nodiscard]] std::uint64_t SubmitRequest(StreamRequest request);

    /// Cancel a pending request (no-op if already loading or loaded).
    void CancelRequest(std::uint64_t requestId);

    // ─── Resource access ──────────────────────────────────────────────────────

    /// Get a loaded resource by path, or nullptr if not loaded.
    /// Thread-safe (shared lock).
    [[nodiscard]] const LoadedResource* GetResource(const std::string& assetPath) const;

    /// Check if a resource is currently loaded.
    [[nodiscard]] bool IsResourceLoaded(const std::string& assetPath) const;

    /// Update the LOD target for a resource. Transition happens over time.
    void SetResourceLodTarget(const std::string& assetPath, std::uint32_t targetLod);

    // ─── Per-frame updates ────────────────────────────────────────────────────

    /// Call once per frame from the main thread.
    /// Processes completion callbacks, updates LOD transitions, enforces budget.
    void Tick(std::uint64_t currentTick, float deltaTimeSec);

    // ─── Diagnostics ──────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t GetPendingRequestCount() const noexcept;
    [[nodiscard]] std::size_t GetLoadedResourceCount() const noexcept;
    [[nodiscard]] std::size_t GetCurrentMemoryUsage() const noexcept;
    [[nodiscard]] std::size_t GetMemoryBudget() const noexcept { return config_.memoryBudgetBytes; }
    [[nodiscard]] std::uint32_t GetActiveWorkerCount() const noexcept;

private:
    StreamingPipelineConfig config_;

    // ─── Request queue ────────────────────────────────────────────────────────

    mutable std::mutex            requestMutex_;
    std::priority_queue<StreamRequest> requestQueue_;
    std::condition_variable       requestCv_;
    std::atomic<bool>             running_{false};
    std::atomic<std::uint64_t>    nextRequestId_{1};

    // ─── Loaded resources ─────────────────────────────────────────────────────

    mutable std::shared_mutex     resourceMutex_;
    std::unordered_map<std::string, LoadedResource> loadedResources_;
    std::size_t currentMemoryUsage_ = 0;

    // ─── Completed requests (main thread processing) ──────────────────────────

    struct CompletionEntry
    {
        std::uint64_t requestId;
        std::string   assetPath;
        void*         data;
        std::size_t   memoryBytes;
        bool          success;
    };

    mutable std::mutex            completionMutex_;
    std::vector<CompletionEntry>  completedRequests_;

    // ─── Worker threads ───────────────────────────────────────────────────────

    std::vector<std::thread>      workerThreads_;
    std::atomic<std::uint32_t>    activeWorkers_{0};

    // ─── Callbacks ────────────────────────────────────────────────────────────

    OnResourceLoadedFn        onLoaded_;
    OnResourceLoadFailedFn    onFailed_;
    OnResourceEvictedFn       onEvicted_;
    LoadResourceFn            loadFn_;
    ReleaseResourceFn         releaseFn_;

    // ─── LOD transition tracking ──────────────────────────────────────────────

    std::uint64_t lastBudgetEnforcementTick_ = 0;

    // ─── Callback registration ────────────────────────────────────────────────

public:
    void OnResourceLoaded(OnResourceLoadedFn fn) { onLoaded_ = std::move(fn); }
    void OnResourceLoadFailed(OnResourceLoadFailedFn fn) { onFailed_ = std::move(fn); }
    void OnResourceEvicted(OnResourceEvictedFn fn) { onEvicted_ = std::move(fn); }
    void SetLoadFunction(LoadResourceFn fn) { loadFn_ = std::move(fn); }
    void SetReleaseFunction(ReleaseResourceFn fn) { releaseFn_ = std::move(fn); }

private:
    // ─── Worker loop ──────────────────────────────────────────────────────────

    void WorkerThreadFunc();

    // ─── Memory management ────────────────────────────────────────────────────

    void EnforceMemoryBudget(std::uint64_t currentTick);

    /// Evict the least-recently-used resource. Returns true if something was evicted.
    bool EvictLruResource();

    // ─── LOD updates ──────────────────────────────────────────────────────────

    void UpdateLodTransitions(float deltaTimeSec, std::uint64_t currentTick);
};

} // namespace SagaEngine::Resources
