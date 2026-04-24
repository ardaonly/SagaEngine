/// @file AudioFilter.cpp
/// @brief Audio event filtering — distance cull, voice limit, dedup, cooldown.

#include "SagaEngine/Audio/AudioFilter.h"

#include <algorithm>
#include <unordered_map>

namespace SagaEngine::Audio
{

AudioFilterStats AudioFilter::Filter(AudioEventBatch& batch)
{
    AudioFilterStats stats{};
    stats.totalIn = static_cast<std::uint32_t>(batch.Size());

    if (batch.Empty())
    {
        m_lastStats = stats;
        return stats;
    }

    std::vector<AudioEvent> accepted;
    accepted.reserve(batch.events.size());

    // ── Pass 1: distance culling ─────────────────────────────────
    for (auto& e : batch.events)
    {
        if (e.distance > m_config.cullDistance &&
            e.priority != AudioPriority::Critical)
        {
            ++stats.culledDistance;
            continue;
        }
        accepted.push_back(std::move(e));
    }

    // ── Pass 2: cooldown check ───────────────────────────────────
    {
        std::vector<AudioEvent> afterCooldown;
        afterCooldown.reserve(accepted.size());

        for (auto& e : accepted)
        {
            if (e.dedupKey != 0)
            {
                auto it = m_cooldownTimers.find(e.dedupKey);
                if (it != m_cooldownTimers.end())
                {
                    // Find cooldown for this category.
                    float cd = 50.f; // default
                    for (auto& cl : m_config.categoryLimits)
                    {
                        if (cl.category == e.category)
                        { cd = cl.cooldownMs; break; }
                    }
                    if ((m_clockMs - it->second) < cd &&
                        e.priority != AudioPriority::Critical)
                    {
                        ++stats.culledCooldown;
                        continue;
                    }
                }
                // Record this event's timestamp.
                m_cooldownTimers[e.dedupKey] = m_clockMs;
            }
            afterCooldown.push_back(std::move(e));
        }
        accepted = std::move(afterCooldown);
    }

    // ── Pass 3: deduplication (same dedupKey in one frame) ───────
    {
        std::unordered_map<std::uint64_t, std::uint32_t> seen;
        std::vector<AudioEvent> afterDedup;
        afterDedup.reserve(accepted.size());

        for (auto& e : accepted)
        {
            if (e.dedupKey != 0)
            {
                auto& count = seen[e.dedupKey];
                if (count >= 1 && e.priority != AudioPriority::Critical)
                {
                    ++stats.culledDedup;
                    continue;
                }
                ++count;
            }
            afterDedup.push_back(std::move(e));
        }
        accepted = std::move(afterDedup);
    }

    // ── Pass 4: per-category voice limiting ──────────────────────
    {
        std::unordered_map<std::uint8_t, std::uint32_t> catCount;
        std::vector<AudioEvent> afterLimit;
        afterLimit.reserve(accepted.size());

        // Sort by priority descending so high-prio events survive the cap.
        std::sort(accepted.begin(), accepted.end(),
                  [](const AudioEvent& a, const AudioEvent& b)
                  { return static_cast<int>(a.priority) > static_cast<int>(b.priority); });

        for (auto& e : accepted)
        {
            auto catKey = static_cast<std::uint8_t>(e.category);
            auto& cnt = catCount[catKey];

            // Find limit for this category.
            std::uint32_t maxPerFrame = 8; // default
            for (auto& cl : m_config.categoryLimits)
            {
                if (cl.category == e.category)
                { maxPerFrame = cl.maxPerFrame; break; }
            }

            if (cnt >= maxPerFrame && e.priority != AudioPriority::Critical)
            {
                ++stats.culledLimit;
                continue;
            }
            ++cnt;
            afterLimit.push_back(std::move(e));
        }
        accepted = std::move(afterLimit);
    }

    // ── Pass 5: global budget cap ────────────────────────────────
    if (accepted.size() > m_config.globalMaxPerFrame)
    {
        std::uint32_t excess = static_cast<std::uint32_t>(
            accepted.size() - m_config.globalMaxPerFrame);
        stats.culledLimit += excess;
        accepted.resize(m_config.globalMaxPerFrame);
    }

    stats.passedOut = static_cast<std::uint32_t>(accepted.size());

    // Write back.
    batch.events = std::move(accepted);
    m_lastStats = stats;
    return stats;
}

void AudioFilter::Tick(float dtMs)
{
    m_clockMs += dtMs;

    // Prune stale cooldown entries every ~5 seconds worth of clock.
    // Keeps the map from growing unbounded in long sessions.
    constexpr float kPruneInterval = 5000.f;
    static float s_lastPrune = 0.f;
    if (m_clockMs - s_lastPrune > kPruneInterval)
    {
        s_lastPrune = m_clockMs;
        for (auto it = m_cooldownTimers.begin(); it != m_cooldownTimers.end(); )
        {
            if ((m_clockMs - it->second) > 2000.f)
                it = m_cooldownTimers.erase(it);
            else
                ++it;
        }
    }
}

void AudioFilter::ResetCooldowns()
{
    m_cooldownTimers.clear();
    m_clockMs = 0.f;
}

} // namespace SagaEngine::Audio
