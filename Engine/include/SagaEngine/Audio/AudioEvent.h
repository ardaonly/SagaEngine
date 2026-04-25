/// @file AudioEvent.h
/// @brief Audio event descriptors — the output of the AudioEventMapper.
///
/// A gameplay event (WeaponFired, Footstep, etc.) is mapped into one or
/// more AudioEvents which carry all the context the backend needs to
/// post the correct Wwise event + set the right RTPCs / switches.

#pragma once

#include "SagaEngine/Audio/AudioTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEngine::Audio
{

/// Fully-resolved audio event ready for the backend.
struct AudioEvent
{
    // ── Identity ─────────────────────────────────────────────────
    std::string    eventName;           ///< Wwise event name, e.g. "Play_AK47_Fire_3P"
    AudioObjectId  gameObject = AudioObjectId::kInvalid;

    // ── Categorisation ───────────────────────────────────────────
    AudioCategory  category  = AudioCategory::SFX;
    AudioPriority  priority  = AudioPriority::Normal;

    // ── Spatial ──────────────────────────────────────────────────
    AudioTransform transform{};
    DistanceBand   distanceBand = DistanceBand::Mid;
    float          distance     = 0.f;   ///< Metres from listener.

    // ── Context (sent to Wwise as RTPCs / switches) ──────────────
    std::vector<RTPCValue>   rtpcs;
    std::vector<AudioSwitch> switches;

    // ── Dedup key ────────────────────────────────────────────────
    /// Hash of (category + source entity) — the filter uses this to
    /// aggregate or drop duplicate events in the same frame.
    std::uint64_t dedupKey = 0;
};

/// A batch of audio events produced by the mapper for one frame.
struct AudioEventBatch
{
    std::vector<AudioEvent> events;

    void Clear() { events.clear(); }
    void Push(AudioEvent e) { events.push_back(std::move(e)); }
    [[nodiscard]] std::size_t Size() const noexcept { return events.size(); }
    [[nodiscard]] bool Empty() const noexcept { return events.empty(); }
};

} // namespace SagaEngine::Audio
