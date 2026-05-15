/// @file SimCell_SpatialLogic.cpp
/// @brief SimCell spatial partitioning logic: ShouldSplit/ShouldMerge implementations.
///
/// Layer  : SagaEngine / World
/// Purpose: Provides threshold-based decision logic for dynamic cell management
///          in the world kernel. Cells split when overloaded, merge when sparse.
///          This file contains only the split/merge decision logic; the actual
///          ExecuteSplit implementation is in SimCell.cpp with position-based
///          partitioning via callback.

#include "SagaEngine/World/SimCell.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cmath>

namespace SagaEngine::World {

// kTag is defined in SimCell.cpp; we use a local one here for this file's logs.
static constexpr const char* kSpatialTag = "SimCell.Spatial";

// ─── Split threshold configuration ──────────────────────────────────────────

/// Configuration for cell split/merge decisions.
/// Tunable per-deployment via config file or command-line.
struct SimCellPartitionConfig
{
    /// Maximum entities before considering split.
    uint32_t maxEntities = 256;

    /// Minimum entities before considering merge (hysteresis).
    uint32_t minEntities = 32;

    /// Maximum players (human-controlled entities) before split.
    uint32_t maxPlayers = 64;

    /// Grace period in world ticks before allowing merge after split.
    uint32_t mergeGraceTicks = 300;  // ~5 seconds at 60Hz

    /// Minimum distance between cell centers to allow merge (prevents oscillation).
    float minMergeDistance = 128.0f;
};

// ─── SimCell::ShouldSplit ───────────────────────────────────────────────────

/// Determine if this cell should split into two child cells.
///
/// Split criteria (any one triggers):
///   1. Entity count exceeds maxEntities threshold
///   2. Player count exceeds maxPlayers threshold
///   3. CPU load metric exceeds configured limit (future)
///
/// External API mapping: SimCell::Metrics → partition decision
///
/// @return true if cell should split, false otherwise.
bool SimCell::ShouldSplit() const noexcept
{
    static constexpr SimCellPartitionConfig kDefaultConfig{};
    const auto& config = kDefaultConfig;
    const auto& metrics = Metrics();

    // Primary trigger: too many total entities.
    if (metrics.entityCount > config.maxEntities)
    {
        LOG_INFO(kSpatialTag, "Cell %d,%d: entity count %u > %u (split candidate)",
                 m_id.worldX, m_id.worldZ,
                 metrics.entityCount, config.maxEntities);
        return true;
    }

    // Secondary trigger: too many players (higher priority than NPCs).
    if (metrics.playerCount > config.maxPlayers)
    {
        LOG_INFO(kSpatialTag, "Cell %d,%d: player count %u > %u (split candidate)",
                 m_id.worldX, m_id.worldZ,
                 metrics.playerCount, config.maxPlayers);
        return true;
    }

    // Future: CPU load, memory pressure, network bandwidth triggers.

    return false;
}

// ─── SimCell::ShouldMerge ───────────────────────────────────────────────────

/// Determine if this cell should merge with an adjacent cell.
///
/// Merge criteria (all must be true):
///   1. Combined entity count <= minEntities * 2 (sparse region)
///   2. Cell has been in Dormant/Active state for >= mergeGraceTicks
///   3. Adjacent cell is also merge-candidate (bidirectional check)
///   4. Distance between centers <= minMergeDistance (prevents oscillation)
///
/// External API mapping: SimCell::Metrics + SimCell::State → partition decision
///
/// @param adjacent   The candidate adjacent cell to merge with.
/// @param config     Partition configuration thresholds.
/// @return true if cells should merge, false otherwise.
bool SimCell::ShouldMerge() const noexcept
{
    static constexpr SimCellPartitionConfig kDefaultConfig{};
    const auto& config = kDefaultConfig;

    // State check: only merge stable cells, not mid-transition.
    if (m_state != SimCellState::Active && m_state != SimCellState::Dormant)
        return false;

    // Hysteresis: don't merge immediately after a split.
    const uint64_t currentTick = m_metrics.lastActivityTick;
    if (currentTick - m_lowStateStartTick < config.mergeGraceTicks)
        return false;

    // Density check: cell must be sparse enough to be a merge candidate.
    const auto& myMetrics = Metrics();
    const uint32_t mergeThreshold = config.minEntities * 2;

    if (myMetrics.entityCount > mergeThreshold)
        return false;

    LOG_INFO(kSpatialTag, "Cell (%d,%d): %u entities <= %u (merge candidate)",
             m_id.worldX, m_id.worldZ,
             myMetrics.entityCount, mergeThreshold);

    return true;
}

} // namespace SagaEngine::World
