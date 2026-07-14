/// @file ResourceStream.h
/// @brief World-level streaming orchestrator that drives the cell loader.
///
/// Layer  : SagaEngine / World / Streaming
/// Purpose: `ResourceStream` is the bridge between the spatial world
///          partition (`Zone`, `Cell`, `Boundary`) and the low-level
///          resource pipeline (`Resources::ResourceManager`,
///          `Resources::StreamingBudget`).  The simulation tick hands
///          it the current camera position; the stream decides which
///          cells should be resident, issues the matching prefetches,
///          drains stale cells, and hands per-entity LOD queries off
///          to the `LODManager` so the renderer never has to reason
///          about residency itself.
///
///          The separation of concerns is deliberate:
///
///            - `ResourceManager` knows about *asset ids*.  It turns
///              an id into decoded bytes and keeps a refcounted LRU.
///            - `ResourceStream` knows about *cells and boundaries*.
///              It maps the player position to a set of cells and
///              issues the right asset ids at the right priority.
///            - `LODManager` knows about *rendering*.  It maps the
///              player position and the streaming budget pressure to
///              a per-entity LOD index.
///
///          Each layer is unit-testable in isolation because the
///          contract between them is a small struct interface rather
///          than a god-object.
///
/// Design rules:
///   - Non-owning references.  The stream borrows the zone, the
///     resource manager, and the budget from the caller; the caller
///     owns the lifetime and must outlive the stream.
///   - Tick-driven.  All state changes happen inside `Tick`; outside
///     callers only observe state through const accessors.
///   - Hysteresis.  Cells are drained lazily over `drainGraceTicks`
///     to avoid thrash when the player walks along a boundary.
///   - Thread affinity.  `Tick` must be called from the simulation
///     thread; the underlying `ResourceManager` is thread-safe, but
///     the stream's cell map is not.
///
/// What this header is NOT:
///   - Not the streaming manager itself.  I/O and worker threads
///     live in `Resources::StreamingManager`; this header only
///     decides *what* to ask for.
///   - Not a culling system.  Whether an entity is drawn is a
///     camera-frustum decision, not a residency decision.

#pragma once

#include "SagaEngine/Resources/ResourceManager.h"
#include "SagaEngine/Resources/StreamingBudget.h"
#include "SagaEngine/World/Partition/Boundary.h"
#include "SagaEngine/World/Partition/Cell.h"
#include "SagaEngine/World/Partition/Zone.h"
#include "SagaEngine/World/Streaming/LODManager.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace SagaEngine::World::Streaming {

// ─── Configuration ─────────────────────────────────────────────────────────

/// Parameters the caller hands to the stream at construction time.
/// Defaults are tuned for an open-world desktop client; mobile
/// profiles tighten the apron and lengthen the drain window.
struct ResourceStreamConfig
{
    /// Apron shape around the focal cell.  Inner ring is mandatory,
    /// outer ring is speculative.
    SagaEngine::World::Partition::BoundaryConfig boundary;

    /// Tuning for the LOD selector.  Forwarded verbatim to the
    /// embedded `LODManager` during construction.
    LodManagerConfig lod;

    /// Hard cap on the number of cells the stream will keep tracked
    /// at once.  Protects the hash table from unbounded growth if a
    /// misconfigured zone tries to load a million cells.
    std::uint32_t maxTrackedCells = 4096;

    /// Logging verbosity.  When enabled, every cell state transition
    /// is logged at `LOG_INFO`; in production this is usually turned
    /// off because a single tick can transition dozens of cells.
    bool logStateTransitions = false;
};

// ─── Per-tick stats ────────────────────────────────────────────────────────

/// Observable counters from the most recent `Tick`.  Consumed by
/// the diagnostics overlay and by tests that want to verify the
/// stream's behaviour without reaching into private state.
struct ResourceStreamTickStats
{
    std::uint32_t cellsPromotedToPrefetching = 0; ///< Dormant → Prefetching this tick.
    std::uint32_t cellsPromotedToLoaded      = 0; ///< Prefetching → Loaded this tick.
    std::uint32_t cellsDemotedToDraining     = 0; ///< Loaded → Draining this tick.
    std::uint32_t cellsEvicted               = 0; ///< Draining → Dormant this tick.
    std::uint32_t prefetchRequestsIssued     = 0; ///< ResourceManager acquires this tick.
    std::uint32_t cellsFailedToPrefetch      = 0; ///< Inner-ring failures this tick.
    float         budgetPressure             = 0.0f; ///< Cached budget pressure at tick start.
};

// ─── ResourceStream ────────────────────────────────────────────────────────

/// World-level streaming orchestrator.  One instance per active
/// zone; the caller creates the stream when a zone becomes the
/// player's current zone and destroys it during shutdown.
class ResourceStream
{
public:
    /// Build a stream bound to the given zone, resource manager,
    /// and budget.  All three references must outlive the stream;
    /// the stream does not take ownership.
    explicit ResourceStream(SagaEngine::World::Partition::Zone&    zone,
                            SagaEngine::Resources::ResourceManager& resourceManager,
                            SagaEngine::Resources::StreamingBudget& budget,
                            ResourceStreamConfig                    config = {}) noexcept;

    ~ResourceStream() noexcept;

    ResourceStream(const ResourceStream&)            = delete;
    ResourceStream& operator=(const ResourceStream&) = delete;
    ResourceStream(ResourceStream&&)                 = delete;
    ResourceStream& operator=(ResourceStream&&)      = delete;

    // ─── Tick driver ─────────────────────────────────────────────────────

    /// Advance the stream by one simulation tick.  `focalWorldX` /
    /// `focalWorldZ` are the player's current world-space XZ
    /// position; `currentTick` is the simulation tick counter and
    /// drives the drain grace period.  Returns the per-tick stats
    /// so the caller can log or display them.
    ResourceStreamTickStats Tick(float         focalWorldX,
                                 float         focalWorldZ,
                                 std::uint64_t currentTick);

    // ─── LOD query ───────────────────────────────────────────────────────

    /// Select a per-entity LOD index.  Delegates to the embedded
    /// `LODManager` with the current budget pressure plugged in;
    /// the caller must still pass the residency floor so the
    /// renderer never draws with data that is not resident.
    [[nodiscard]] std::uint8_t SelectLod(const LodQuery& query) const noexcept;

    // ─── Introspection ───────────────────────────────────────────────────

    /// Borrow the most recent tick stats.
    [[nodiscard]] const ResourceStreamTickStats& LastTickStats() const noexcept
    {
        return lastTickStats_;
    }

    /// Number of cells currently tracked (non-Dormant) by the
    /// stream.  Diagnostic helper; the per-state breakdown lives
    /// in `LastTickStats`.
    [[nodiscard]] std::size_t TrackedCellCount() const noexcept
    {
        return trackedCells_.size();
    }

    /// Borrow the underlying LOD manager for configuration tweaks
    /// (for example, the in-editor "simulate low-end hardware"
    /// toggle reconfigures the thresholds at runtime).
    [[nodiscard]] LODManager&       Lod() noexcept       { return lodManager_; }
    [[nodiscard]] const LODManager& Lod() const noexcept { return lodManager_; }

private:
    // ─── Internal helpers ────────────────────────────────────────────────

    /// State carried per tracked cell on the stream's side.  The
    /// authoritative cell descriptor lives in the `Zone`; this
    /// struct holds only the live residency handles so the stream
    /// can refcount them.
    struct TrackedCell
    {
        SagaEngine::World::Partition::CellCoord coord;
        SagaEngine::World::Partition::CellState state      = SagaEngine::World::Partition::CellState::Dormant;
        SagaEngine::World::Partition::BoundaryRing lastRing =
            SagaEngine::World::Partition::BoundaryRing::Outside;
        std::uint64_t lastSeenTick = 0;
        std::vector<SagaEngine::Resources::ResidencyHandle> residencyHandles;
    };

    /// Mark a cell as entering the focal region.  Creates the
    /// tracked-cell entry if necessary and issues the initial
    /// prefetch pass.
    void PromoteCellToPrefetching(const SagaEngine::World::Partition::Cell& cell,
                                  SagaEngine::World::Partition::BoundaryRing ring,
                                  std::uint64_t                              currentTick,
                                  ResourceStreamTickStats&                   stats);

    /// Drain a tracked cell that is no longer inside either ring.
    /// The actual eviction (releasing residency handles) is deferred
    /// until `drainGraceTicks` have elapsed.
    void BeginDrainingCell(TrackedCell&  tracked,
                           std::uint64_t currentTick,
                           ResourceStreamTickStats& stats);

    /// Final eviction pass — drops residency handles and removes
    /// the tracked-cell entry from the map.
    void EvictCell(std::uint64_t               key,
                   ResourceStreamTickStats&    stats);

    /// Acquire one asset through the resource manager at the given
    /// priority and append the residency handle to `tracked`.
    void AcquireAsset(TrackedCell&                                      tracked,
                      const SagaEngine::World::Partition::CellAssetRef& ref,
                      SagaEngine::Resources::StreamingPriority          priority,
                      ResourceStreamTickStats&                          stats);

    /// Query the budget for its current pressure ratio.  Centralised
    /// so the Tick and the LOD query agree on the same value.
    [[nodiscard]] float CurrentPressure() const noexcept;

    // ─── Members ─────────────────────────────────────────────────────────

    SagaEngine::World::Partition::Zone&     zone_;
    SagaEngine::Resources::ResourceManager& resourceManager_;
    SagaEngine::Resources::StreamingBudget& budget_;
    ResourceStreamConfig                    config_;
    LODManager                              lodManager_;

    /// Map from packed cell key → tracked cell state.  The map is
    /// keyed by `Partition::PackCellCoord` so lookups are O(1).
    std::unordered_map<std::uint64_t, TrackedCell> trackedCells_;

    /// Stats from the most recent `Tick` call.  Cleared at the
    /// start of every tick.
    ResourceStreamTickStats lastTickStats_;
};

} // namespace SagaEngine::World::Streaming
