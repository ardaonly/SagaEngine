/// @file LODManager.cpp
/// @brief Distance- and pressure-driven LOD selection logic.

#include "SagaEngine/World/Streaming/LODManager.h"

#include <algorithm>

namespace SagaEngine::World::Streaming {

// ─── Select (raw pressure) ─────────────────────────────────────────────────

std::uint8_t LODManager::Select(const LodQuery& query,
                                float           pressureRatio) const noexcept
{
    // A pinned entity wins unconditionally — cinematic cameras and
    // quest targets never accept a coarser LOD.  The residency floor
    // still clamps the answer downward because the renderer cannot
    // draw with data that is not in memory.
    if (query.pinnedLod <= kMaxLodIndex)
    {
        return std::max<std::uint8_t>(query.pinnedLod, query.residencyFloor);
    }

    // Walk the thresholds in order.  The first slot whose squared
    // threshold exceeds the query's squared distance is the base
    // LOD; everything beyond the last slot drops to the coarsest.
    std::uint8_t baseLod = kMaxLodIndex;
    for (std::uint8_t i = 0; i < kMaxMeshLods; ++i)
    {
        if (query.distanceSq <= config_.distanceSqThresholds[i])
        {
            baseLod = i;
            break;
        }
    }

    // Apply pressure bias.  The dead zone keeps the bias at zero for
    // gentle pressure so a scene that idles just above the soft limit
    // does not flicker between LODs every frame.
    std::uint8_t pressureBias = 0;
    if (config_.maxPressureBias > 0 && pressureRatio > config_.pressureBiasDeadZone)
    {
        const float denom = 1.0f - config_.pressureBiasDeadZone;
        const float normalised = denom > 0.0f
                                     ? (pressureRatio - config_.pressureBiasDeadZone) / denom
                                     : 1.0f;
        const float clamped = normalised < 0.0f ? 0.0f : (normalised > 1.0f ? 1.0f : normalised);
        pressureBias = static_cast<std::uint8_t>(
            static_cast<float>(config_.maxPressureBias) * clamped + 0.5f);
    }

    // Combine base + bias + residency floor.  All three are clamped
    // to `kMaxLodIndex` so a misconfigured array cannot produce an
    // out-of-range LOD.
    const std::uint16_t combined =
        static_cast<std::uint16_t>(baseLod) +
        static_cast<std::uint16_t>(pressureBias);
    const std::uint8_t biased = combined > kMaxLodIndex
                                    ? kMaxLodIndex
                                    : static_cast<std::uint8_t>(combined);

    return std::max<std::uint8_t>(biased, query.residencyFloor);
}

// ─── Select (budget overload) ──────────────────────────────────────────────

std::uint8_t LODManager::Select(
    const LodQuery&                               query,
    const SagaEngine::Resources::StreamingBudget& budget) const noexcept
{
    return Select(query, budget.Pressure());
}

} // namespace SagaEngine::World::Streaming
