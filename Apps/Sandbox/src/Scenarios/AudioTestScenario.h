/// @file AudioTestScenario.h
/// @brief Sandbox scenario that exercises the full audio pipeline end-to-end.
///
/// Layer  : Sandbox / Scenarios / Catalog
/// Purpose: Tests every stage of the audio system:
///   Server Event (gameplay intent) → AudioEventMapper → AudioFilter → AudioQueue → IAudioBackend
///
/// Features exercised:
///   - AudioEngine initialisation with NullAudioBackend (no Wwise required)
///   - Default mapping rules (WeaponFired, Footstep, SpellCast, EntityDied, Impact)
///   - Custom user-registered mapping rules
///   - AudioFilter with distance culling, dedup, cooldown, per-category limits
///   - Live ImGui panels showing: emitter grid, filter stats, queue depth, event log
///   - Simulated 3D emitter grid with moving listener

#pragma once

#include "SagaSandbox/Core/IScenario.h"

#include <SagaEngine/Audio/AudioEngine.h>
#include <SagaEngine/Audio/AudioTypes.h>
#include <SagaEngine/Math/Vec3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace SagaSandbox
{

class AudioTestScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "audio_test",
        .displayName = "Audio Pipeline Test",
        .category    = "Audio",
        .description = "Exercises the full audio pipeline: intent mapping, filtering, queue, backend dispatch.",
    };

    AudioTestScenario()  = default;
    ~AudioTestScenario() override = default;

    // ── IScenario ─────────────────────────────────────────────────────────────

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    {
        return kDescriptor;
    }

    [[nodiscard]] bool OnInit()                          override;
    void               OnUpdate(float dt, uint64_t tick) override;
    void               OnShutdown()                      override;
    void               OnRenderDebugUI()                 override;

private:
    // ── ImGui panels ──────────────────────────────────────────────────────────

    void RenderEmitterPanel();
    void RenderFilterStatsPanel();
    void RenderEventLogPanel();
    void RenderControlPanel();

    // ── Audio engine ──────────────────────────────────────────────────────────

    SagaEngine::Audio::AudioEngine m_audioEngine;

    // ── Simulated 3D world ────────────────────────────────────────────────────

    struct AudioEmitter
    {
        SagaEngine::Audio::AudioObjectId id = SagaEngine::Audio::AudioObjectId::kInvalid;
        SagaEngine::Math::Vec3           position{};
        std::string                      label;
        float                            cooldown = 0.f;    ///< Seconds until next auto-fire.
        float                            interval = 1.f;    ///< Seconds between auto-fires.
        std::string                      intentType = "WeaponFired";
        SagaEngine::Audio::SurfaceType   surface = SagaEngine::Audio::SurfaceType::Default;
        bool                             autoFire = true;
    };

    std::vector<AudioEmitter> m_emitters;

    // ── Listener ──────────────────────────────────────────────────────────────

    SagaEngine::Math::Vec3 m_listenerPos{0.f, 0.f, 0.f};
    float                  m_listenerAngle = 0.f;   ///< Radians, orbits around origin.
    bool                   m_listenerOrbiting = true;
    float                  m_orbitRadius = 10.f;
    float                  m_orbitSpeed  = 0.5f;     ///< Rad/sec.

    // ── Event log ring buffer ─────────────────────────────────────────────────

    struct EventLogEntry
    {
        float       timestamp = 0.f;
        std::string text;
    };

    std::vector<EventLogEntry> m_eventLog;
    static constexpr std::size_t kMaxLogEntries = 128;

    float m_clock = 0.f;

    // ── Control state ─────────────────────────────────────────────────────────

    bool  m_paused       = false;
    float m_spamRate     = 0.f;     ///< Extra intents/sec stress test.
    int   m_spamCategory = 0;       ///< 0=Weapon, 1=Footstep, 2=Spell, 3=Impact
    std::uint32_t m_totalIntentsPosted   = 0;
    std::uint32_t m_totalEventsDispatched = 0;
};

} // namespace SagaSandbox
