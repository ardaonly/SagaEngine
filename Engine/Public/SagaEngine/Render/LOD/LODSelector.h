/// @file LODSelector.h
/// @brief Distance-based LOD level selection with quality presets and bias.
///
/// Layer  : SagaEngine / Render / LOD
/// Purpose: Picks a LOD index for a drawable given the camera distance and
///          a set of per-LOD distance thresholds. The selector is the ONE
///          place client quality settings influence culling. Server never
///          instantiates this type.
///
/// Design rules:
///   - Pure function over (thresholds, distance, quality, camera lodBias).
///   - Thresholds are sorted ascending. LOD 0 (highest detail) uses 0.0;
///     LOD N is picked when distance >= thresholds[N].
///   - Quality preset multiplies every threshold — "Ultra" pushes LODs
///     further out (draw high detail longer), "Low" pulls them closer
///     (draw cheap LODs earlier).
///   - Never allocates.

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace SagaEngine::Render::LOD
{

// ─── Quality preset ───────────────────────────────────────────────────

/// Coarse client-side quality preset. Multiplies LOD distance thresholds.
enum class QualityPreset : std::uint8_t
{
    Low    = 0,   ///< Multiplier 0.5 — swap to cheap LODs earlier.
    Medium = 1,   ///< Multiplier 0.75.
    High   = 2,   ///< Multiplier 1.0 (authored thresholds).
    Ultra  = 3,   ///< Multiplier 1.5 — keep high detail for longer.
};

/// Returns the preset's threshold multiplier.
[[nodiscard]] inline constexpr float QualityScale(QualityPreset q) noexcept
{
    switch (q)
    {
        case QualityPreset::Low:    return 0.5f;
        case QualityPreset::Medium: return 0.75f;
        case QualityPreset::High:   return 1.0f;
        case QualityPreset::Ultra:  return 1.5f;
    }
    return 1.0f;
}

// ─── Thresholds ───────────────────────────────────────────────────────

/// Maximum supported LOD levels. Matches MeshAsset's 4-LOD cap.
inline constexpr std::uint8_t kMaxLodLevels = 4;

/// Per-LOD camera-distance thresholds. Index 0 is always 0.0 (highest
/// detail); index N is the distance at which LOD N becomes preferred.
struct LodThresholds
{
    std::array<float, kMaxLodLevels> byLevel{0.0f, 0.0f, 0.0f, 0.0f};
    std::uint8_t                     count = 1;  ///< 1..kMaxLodLevels.
};

// ─── Selector ─────────────────────────────────────────────────────────

/// Select the LOD index whose threshold is the largest <= scaledDistance.
/// `distance` is world-space units (not squared). `cameraLodBias` is a
/// multiplicative pull / push (from Camera::lodBias).
[[nodiscard]] inline std::uint8_t SelectLod(const LodThresholds& thresholds,
                                              float distance,
                                              QualityPreset preset,
                                              float cameraLodBias = 1.0f) noexcept
{
    const std::uint8_t count = std::min<std::uint8_t>(thresholds.count, kMaxLodLevels);
    if (count == 0) return 0;

    const float scale = QualityScale(preset) * cameraLodBias;

    std::uint8_t chosen = 0;
    for (std::uint8_t i = 1; i < count; ++i)
    {
        const float scaled = thresholds.byLevel[i] * scale;
        if (distance >= scaled)
            chosen = i;
        else
            break;
    }
    return chosen;
}

/// Convenience: select from a squared distance (avoids a sqrt at call sites
/// that already have it). `thresholds` are compared after squaring and
/// scaling appropriately.
[[nodiscard]] inline std::uint8_t SelectLodSq(const LodThresholds& thresholds,
                                                float distanceSq,
                                                QualityPreset preset,
                                                float cameraLodBias = 1.0f) noexcept
{
    const std::uint8_t count = std::min<std::uint8_t>(thresholds.count, kMaxLodLevels);
    if (count == 0) return 0;

    const float scale  = QualityScale(preset) * cameraLodBias;
    const float scale2 = scale * scale;

    std::uint8_t chosen = 0;
    for (std::uint8_t i = 1; i < count; ++i)
    {
        const float t       = thresholds.byLevel[i];
        const float scaled2 = t * t * scale2;
        if (distanceSq >= scaled2)
            chosen = i;
        else
            break;
    }
    return chosen;
}

} // namespace SagaEngine::Render::LOD
