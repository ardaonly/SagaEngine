/// @file AudioFilter.h
/// @brief Filters, prioritises, and deduplicates audio events before dispatch.
///
/// Solves the "200 footsteps at once" problem in MMOs.  Runs after
/// AudioEventMapper and before IAudioBackend dispatch.
///
/// Responsibilities:
///   1. Distance culling — drop events beyond kDefaultCullDistance.
///   2. Per-category voice limiting — e.g. max 5 explosions per frame.
///   3. Deduplication / aggregation — group identical events, keep N.
///   4. Cooldown — same event from same source within X ms is dropped.
///   5. Priority sort — if we exceed global budget, lowest priority dies.

#pragma once

#include "SagaEngine/Audio/AudioEvent.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Audio
{

/// Per-category limit configuration.
struct CategoryLimit
{
    AudioCategory  category   = AudioCategory::SFX;
    std::uint32_t  maxPerFrame = 8;
    float          cooldownMs  = 50.f;   ///< Minimum ms between same-source events.
};

/// Configuration for the audio filter.
struct AudioFilterConfig
{
    float           cullDistance     = kDefaultCullDistance;
    std::uint32_t   globalMaxPerFrame = kMaxConcurrentVoices;
    std::vector<CategoryLimit> categoryLimits;
};

/// Per-frame statistics after filtering.
struct AudioFilterStats
{
    std::uint32_t totalIn       = 0;
    std::uint32_t passedOut     = 0;
    std::uint32_t culledDistance = 0;
    std::uint32_t culledLimit   = 0;
    std::uint32_t culledDedup   = 0;
    std::uint32_t culledCooldown = 0;
};

class AudioFilter
{
public:
    AudioFilter() = default;
    explicit AudioFilter(const AudioFilterConfig& cfg) : m_config(cfg) {}

    void SetConfig(const AudioFilterConfig& cfg) { m_config = cfg; }

    /// Filter a batch in-place.  Returns the stats for this frame.
    AudioFilterStats Filter(AudioEventBatch& batch);

    /// Call once per frame to advance cooldown timers.
    void Tick(float dtMs);

    /// Reset all cooldown state (e.g. on zone transition).
    void ResetCooldowns();

    [[nodiscard]] const AudioFilterStats& LastStats() const noexcept { return m_lastStats; }

private:
    AudioFilterConfig m_config{};
    AudioFilterStats  m_lastStats{};

    /// Tracks last-play time per dedupKey for cooldown enforcement.
    std::unordered_map<std::uint64_t, float> m_cooldownTimers;
    float m_clockMs = 0.f;
};

} // namespace SagaEngine::Audio
