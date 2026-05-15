/// @file DomainScheduler.h
/// @brief Multi-rate tick scheduler for simulation domains.
///
/// Layer  : SagaEngine / World
/// Purpose: The world does not simulate at a single frequency.  Different
///          systems have different update needs:
///
///            Combat     → 60 Hz (projectiles, hit detection, cooldowns)
///            Movement   → 20 Hz (position updates, collision checks)
///            Economy    →  1 Hz (market prices, resource production)
///            Politics   →  1 / min (guild influence, territory control)
///            Ecology    →  1 / hr  (weather, crops, NPC migration)
///            Narrative  → Event-driven (story triggers, NPC dialogue)
///
///          Running everything at 60 Hz wastes CPU.  Running everything
///          at 1 Hz feels laggy.  The DomainScheduler runs each domain
///          at its own tick rate, accumulating sub-tick deltas so that
///          slower domains still advance correctly over time.
///
/// Design rules:
///   - Each domain has a tick interval in "world ticks" (1 world tick = 1/60s)
///   - The scheduler accumulates fractional ticks so a 1 Hz domain runs
///     exactly once per 60 world ticks, not "roughly"
///   - Domains are registered by the WorldNode at startup
///   - Cells register with domains they care about via SimCell::RegisterDomains
///   - The WorldNode calls Tick() which returns which domains fired, then
///     the WorldNode dispatches the actual cell-level tick callbacks
///
/// What this is NOT:
///   - Not a thread pool.  Domains run sequentially on the simulation thread.
///     Parallel domain execution is a future optimization.
///   - Not a task graph.  Dependencies between domains are implicit (combat
///     reads positions set by movement).

#pragma once

#include "SagaEngine/World/SimCell.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::World {

// ─── Domain descriptor ──────────────────────────────────────────────────────────

struct SimDomainDescriptor
{
    SimDomainType   type            = SimDomainType::Combat;
    const char*     name            = "unnamed";
    uint32_t        ticksPerSecond  = 60;  ///< Target frequency.
    uint32_t        cellCount       = 0;   ///< How many cells registered with this domain.
};

// ─── Domain scheduler ──────────────────────────────────────────────────────────

/// Manages multi-rate ticking for all registered simulation domains.
///
/// Usage:
///   1. WorldNode creates a DomainScheduler
///   2. Register domains with target frequencies
///   3. Register cells with domains
///   4. Each world tick, call scheduler.Tick() → get list of firing domains
///   5. WorldNode dispatches tick callbacks for those domains
///
/// Thread model: single-threaded, called from the WorldNode's simulation thread.
class DomainScheduler
{
public:
    DomainScheduler() = default;
    ~DomainScheduler() = default;

    DomainScheduler(const DomainScheduler&)            = delete;
    DomainScheduler& operator=(const DomainScheduler&) = delete;

    // ─── Domain registration ──────────────────────────────────────────────────

    /// Register a simulation domain.  Must be called before Tick().
    /// Overwriting an existing domain of the same type is allowed (hot reload).
    void RegisterDomain(SimDomainType type, const char* name,
                        uint32_t ticksPerSecond) noexcept;

    /// Unregister a domain.  Cells remain; they just won't receive ticks.
    void UnregisterDomain(SimDomainType type) noexcept;

    // ─── Cell registration ────────────────────────────────────────────────────

    /// Register a cell with a domain.  The scheduler will list this cell
    /// when the domain fires during Tick().
    void RegisterCell(SimCellId cellId, SimDomainType domain) noexcept;

    /// Unregister a cell from a domain.
    void UnregisterCell(SimCellId cellId, SimDomainType domain) noexcept;

    /// Unregister a cell from ALL domains (used when cell is destroyed or migrated).
    void UnregisterCellAll(SimCellId cellId) noexcept;

    // ─── World tick ───────────────────────────────────────────────────────────

    /// Advance the world by one tick.  Returns the list of domain types
    /// that fired on this tick.  The WorldNode should then dispatch the
    /// domain tick for each registered cell of those domains.
    ///
    /// @param worldTick  Monotonic world tick counter (1/60s).
    /// @param outFiringDomains  Output: which domains fired this tick.
    void Tick(uint64_t worldTick, std::vector<SimDomainType>& outFiringDomains) noexcept;

    // ─── Queries ──────────────────────────────────────────────────────────────

    [[nodiscard]] const SimDomainDescriptor* GetDomain(SimDomainType type) const noexcept;
    [[nodiscard]] uint32_t RegisteredCellCount(SimDomainType type) const noexcept;

    struct DomainStats
    {
        uint64_t totalTicksFired   = 0;  ///< Total domain tick callbacks fired.
        uint64_t lastFireWorldTick = 0;  ///< World tick when this domain last fired.
        float    subTickAccumulator = 0; ///< 0.0–1.0, progress toward next fire.
    };

    [[nodiscard]] const DomainStats& GetDomainStats(SimDomainType type) const noexcept;

    /// Return all cells registered with a domain (for WorldNode dispatch).
    [[nodiscard]] const std::vector<SimCellId>& GetDomainCells(SimDomainType type) const noexcept;

private:
    struct DomainState
    {
        SimDomainDescriptor descriptor{};
        uint64_t            accumulator  = 0;  ///< Fractional tick accumulator (in 1/60s units).
        uint32_t            interval     = 1;  ///< Ticks between fires = 60 / ticksPerSecond.
        DomainStats         stats{};
        bool                active       = false;
    };

    // One entry per domain type.
    std::array<DomainState, static_cast<size_t>(SimDomainType::Count)> m_domains{};

    // Per-domain cell registry: domain → list of cell IDs.
    std::array<std::vector<SimCellId>, static_cast<size_t>(SimDomainType::Count)> m_domainCells{};
};

} // namespace SagaEngine::World
