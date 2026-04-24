/// @file AudioTestScenario.cpp
/// @brief Audio pipeline test scenario — exercises the full intent→dispatch chain.

#include "Scenarios/AudioTestScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(AudioTestScenario);

static constexpr const char* kTag = "AudioTestScenario";
static constexpr float kPi = 3.14159265358979323846f;

// ═════════════════════════════════════════════════════════════════════════════
//  Lifecycle
// ═════════════════════════════════════════════════════════════════════════════

bool AudioTestScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising audio test scenario...");

    // ── Configure the filter ─────────────────────────────────────────────
    SagaEngine::Audio::AudioFilterConfig filterCfg;
    filterCfg.cullDistance       = 80.f;
    filterCfg.globalMaxPerFrame = 32;
    filterCfg.categoryLimits    = {
        { SagaEngine::Audio::AudioCategory::Weapon,   5, 100.f },
        { SagaEngine::Audio::AudioCategory::Footstep, 3,  80.f },
        { SagaEngine::Audio::AudioCategory::Spell,    4, 150.f },
        { SagaEngine::Audio::AudioCategory::Impact,   6,  60.f },
        { SagaEngine::Audio::AudioCategory::SFX,      8,  50.f },
    };

    SagaEngine::Audio::AudioEngineConfig cfg;
    cfg.filterConfig   = filterCfg;
    cfg.useNullBackend = true;   // No Wwise needed — NullAudioBackend.

    if (!m_audioEngine.Initialize(cfg))
    {
        LOG_ERROR(kTag, "AudioEngine init failed!");
        return false;
    }

    // ── Register a custom mapping rule: "Explosion" ──────────────────────
    m_audioEngine.GetMapper().Register("Explosion",
        [](const SagaEngine::Audio::GameplayAudioIntent& intent,
           const SagaEngine::Audio::ListenerContext& /*ctx*/,
           SagaEngine::Audio::AudioEventBatch& out)
        {
            SagaEngine::Audio::AudioEvent e;
            e.eventName  = "Play_Explosion";
            e.gameObject = intent.sourceObject;
            e.category   = SagaEngine::Audio::AudioCategory::SFX;
            e.priority   = SagaEngine::Audio::AudioPriority::Critical;  // never culled
            e.transform.position = intent.sourceTransform.position;
            e.distance   = 0.f;   // will be re-computed, but intent is self-contained here
            out.Push(std::move(e));
        });

    // ── Spawn emitters in a grid ─────────────────────────────────────────
    auto nextId = [n = std::uint64_t{1}]() mutable {
        return static_cast<SagaEngine::Audio::AudioObjectId>(n++);
    };

    // 4 weapon emitters at cardinal directions, distance 20m
    const char* weaponLabels[] = { "Turret N", "Turret E", "Turret S", "Turret W" };
    float angles[] = { 0.f, kPi * 0.5f, kPi, kPi * 1.5f };
    for (int i = 0; i < 4; ++i)
    {
        AudioEmitter em;
        em.id       = nextId();
        em.position = { std::cos(angles[i]) * 20.f, 0.f, std::sin(angles[i]) * 20.f };
        em.label    = weaponLabels[i];
        em.interval = 0.8f + static_cast<float>(i) * 0.3f;
        em.intentType = "WeaponFired";
        m_emitters.push_back(em);
        m_audioEngine.RegisterObject(em.id);
    }

    // 6 footstep emitters, scattered closer (5-15m)
    for (int i = 0; i < 6; ++i)
    {
        float a = (static_cast<float>(i) / 6.f) * 2.f * kPi;
        float r = 5.f + static_cast<float>(i % 3) * 5.f;

        AudioEmitter em;
        em.id       = nextId();
        em.position = { std::cos(a) * r, 0.f, std::sin(a) * r };
        em.label    = "Walker " + std::to_string(i);
        em.interval = 0.4f + static_cast<float>(i) * 0.05f;
        em.intentType = "Footstep";
        em.surface  = static_cast<SagaEngine::Audio::SurfaceType>(
                          static_cast<int>(SagaEngine::Audio::SurfaceType::Stone) + (i % 5));
        m_emitters.push_back(em);
        m_audioEngine.RegisterObject(em.id);
    }

    // 2 spell casters at distance 30m
    for (int i = 0; i < 2; ++i)
    {
        float a = kPi * 0.25f + static_cast<float>(i) * kPi;
        AudioEmitter em;
        em.id       = nextId();
        em.position = { std::cos(a) * 30.f, 0.f, std::sin(a) * 30.f };
        em.label    = "Mage " + std::to_string(i);
        em.interval = 2.0f;
        em.intentType = "SpellCastStarted";
        m_emitters.push_back(em);
        m_audioEngine.RegisterObject(em.id);
    }

    // 1 far explosion emitter at 90m (should be culled by distance)
    {
        AudioEmitter em;
        em.id       = nextId();
        em.position = { 90.f, 0.f, 0.f };
        em.label    = "Far Explosion";
        em.interval = 3.0f;
        em.intentType = "Explosion";
        m_emitters.push_back(em);
        m_audioEngine.RegisterObject(em.id);
    }

    // 1 impact emitter right next to the listener path
    {
        AudioEmitter em;
        em.id       = nextId();
        em.position = { 8.f, 0.f, 0.f };
        em.label    = "Impact Point";
        em.interval = 1.5f;
        em.intentType = "Impact";
        em.surface  = SagaEngine::Audio::SurfaceType::Metal;
        m_emitters.push_back(em);
        m_audioEngine.RegisterObject(em.id);
    }

    LOG_INFO(kTag, "Spawned %zu audio emitters, listener orbiting at r=%.0f",
             m_emitters.size(), m_orbitRadius);

    return true;
}

void AudioTestScenario::OnUpdate(float dt, [[maybe_unused]] uint64_t tick)
{
    if (m_paused) return;

    m_clock += dt;

    // ── Move listener in orbit ───────────────────────────────────────────
    if (m_listenerOrbiting)
    {
        m_listenerAngle += m_orbitSpeed * dt;
        if (m_listenerAngle > 2.f * kPi) m_listenerAngle -= 2.f * kPi;
    }
    m_listenerPos = {
        std::cos(m_listenerAngle) * m_orbitRadius,
        0.f,
        std::sin(m_listenerAngle) * m_orbitRadius
    };

    SagaEngine::Audio::ListenerContext lctx;
    lctx.listenerTransform.position = m_listenerPos;
    lctx.listenerTransform.forward  = { -std::cos(m_listenerAngle), 0.f, -std::sin(m_listenerAngle) };
    lctx.listenerTransform.up       = { 0.f, 1.f, 0.f };
    lctx.environment    = SagaEngine::Audio::EnvironmentType::Outdoor;
    lctx.isFirstPerson  = true;
    lctx.isInMenu       = false;
    lctx.masterVolume   = 1.f;
    m_audioEngine.SetListenerContext(lctx);

    // ── Update emitter transforms ────────────────────────────────────────
    for (auto& em : m_emitters)
    {
        SagaEngine::Audio::AudioTransform t;
        t.position = em.position;
        m_audioEngine.SetObjectTransform(em.id, t);
    }

    // ── Auto-fire emitters ───────────────────────────────────────────────
    for (auto& em : m_emitters)
    {
        if (!em.autoFire) continue;

        em.cooldown -= dt;
        if (em.cooldown <= 0.f)
        {
            em.cooldown = em.interval;

            SagaEngine::Audio::GameplayAudioIntent intent;
            intent.type = em.intentType;
            intent.sourceObject = em.id;
            intent.sourceTransform.position = em.position;
            intent.surfaceType = em.surface;
            intent.intensity   = 0.8f;
            intent.weaponId    = 47;   // AK-47 as test
            intent.spellId     = 101;

            m_audioEngine.PostIntent(intent);
            ++m_totalIntentsPosted;

            // Log it
            if (m_eventLog.size() < kMaxLogEntries)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "[%.1fs] %s fired '%s' @ (%.0f, %.0f)",
                              m_clock, em.label.c_str(), em.intentType.c_str(),
                              em.position.x, em.position.z);
                m_eventLog.push_back({ m_clock, buf });
            }
            else
            {
                // Ring buffer: overwrite oldest
                auto idx = m_totalIntentsPosted % kMaxLogEntries;
                char buf[256];
                std::snprintf(buf, sizeof(buf), "[%.1fs] %s fired '%s' @ (%.0f, %.0f)",
                              m_clock, em.label.c_str(), em.intentType.c_str(),
                              em.position.x, em.position.z);
                m_eventLog[idx] = { m_clock, buf };
            }
        }
    }

    // ── Spam stress test ─────────────────────────────────────────────────
    if (m_spamRate > 0.f)
    {
        int spamCount = static_cast<int>(m_spamRate * dt);
        if (spamCount < 1 && m_spamRate > 0.f) spamCount = 1; // at least 1

        const char* spamTypes[] = { "WeaponFired", "Footstep", "SpellCastStarted", "Impact" };

        for (int i = 0; i < spamCount; ++i)
        {
            SagaEngine::Audio::GameplayAudioIntent intent;
            intent.type = spamTypes[m_spamCategory % 4];
            intent.sourceObject = static_cast<SagaEngine::Audio::AudioObjectId>(
                                      900 + (m_totalIntentsPosted % 50));
            intent.sourceTransform.position = {
                static_cast<float>((m_totalIntentsPosted * 7) % 100) - 50.f,
                0.f,
                static_cast<float>((m_totalIntentsPosted * 13) % 100) - 50.f
            };
            intent.intensity = 0.5f;
            m_audioEngine.PostIntent(intent);
            ++m_totalIntentsPosted;
        }
    }

    // ── Process audio pipeline ───────────────────────────────────────────
    m_audioEngine.Update(dt);

    // Count dispatched events (before dispatch clears the queue).
    m_totalEventsDispatched += static_cast<std::uint32_t>(m_audioEngine.QueueDepth());

    m_audioEngine.DispatchToBackend();
}

void AudioTestScenario::OnShutdown()
{
    LOG_INFO(kTag, "Shutting down audio test scenario...");

    for (auto& em : m_emitters)
        m_audioEngine.UnregisterObject(em.id);

    m_emitters.clear();
    m_eventLog.clear();
    m_audioEngine.Shutdown();

    LOG_INFO(kTag, "Audio test scenario shut down. Total intents posted: %u, dispatched: %u",
             m_totalIntentsPosted, m_totalEventsDispatched);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Debug UI
// ═════════════════════════════════════════════════════════════════════════════

void AudioTestScenario::OnRenderDebugUI()
{
    RenderControlPanel();
    RenderEmitterPanel();
    RenderFilterStatsPanel();
    RenderEventLogPanel();
}

// ── Control Panel ────────────────────────────────────────────────────────────

void AudioTestScenario::RenderControlPanel()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 260), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio Controls"))
    {
        ImGui::Checkbox("Paused", &m_paused);
        ImGui::Separator();

        ImGui::Text("Listener");
        ImGui::Checkbox("Orbiting", &m_listenerOrbiting);
        ImGui::SliderFloat("Orbit Radius", &m_orbitRadius, 2.f, 50.f);
        ImGui::SliderFloat("Orbit Speed", &m_orbitSpeed, 0.1f, 3.f);
        ImGui::Text("Position: (%.1f, %.1f, %.1f)", m_listenerPos.x, m_listenerPos.y, m_listenerPos.z);

        ImGui::Separator();
        ImGui::Text("Stress Test");
        ImGui::SliderFloat("Spam Rate (evt/sec)", &m_spamRate, 0.f, 500.f);
        const char* spamLabels[] = { "Weapon", "Footstep", "Spell", "Impact" };
        ImGui::Combo("Spam Category", &m_spamCategory, spamLabels, 4);

        ImGui::Separator();
        ImGui::Text("Totals");
        ImGui::Text("  Intents posted:    %u", m_totalIntentsPosted);
        ImGui::Text("  Events dispatched: %u", m_totalEventsDispatched);
        ImGui::Text("  Queue depth:       %zu", m_audioEngine.QueueDepth());
        ImGui::Text("  Clock:             %.1f s", m_clock);
    }
    ImGui::End();
}

// ── Emitter Panel ────────────────────────────────────────────────────────────

void AudioTestScenario::RenderEmitterPanel()
{
    ImGui::SetNextWindowPos(ImVec2(10, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 300), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio Emitters"))
    {
        ImGui::Columns(5, "emitters");
        ImGui::SetColumnWidth(0, 100);
        ImGui::SetColumnWidth(1, 100);
        ImGui::SetColumnWidth(2, 80);
        ImGui::SetColumnWidth(3, 80);
        ImGui::SetColumnWidth(4, 60);

        ImGui::Text("Label");      ImGui::NextColumn();
        ImGui::Text("Type");       ImGui::NextColumn();
        ImGui::Text("Pos XZ");     ImGui::NextColumn();
        ImGui::Text("Dist");       ImGui::NextColumn();
        ImGui::Text("Auto");       ImGui::NextColumn();
        ImGui::Separator();

        for (std::size_t i = 0; i < m_emitters.size(); ++i)
        {
            auto& em = m_emitters[i];
            float dx = em.position.x - m_listenerPos.x;
            float dz = em.position.z - m_listenerPos.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            ImGui::Text("%s", em.label.c_str());          ImGui::NextColumn();
            ImGui::Text("%s", em.intentType.c_str());     ImGui::NextColumn();
            ImGui::Text("%.0f,%.0f", em.position.x, em.position.z); ImGui::NextColumn();
            ImGui::Text("%.1fm", dist);                   ImGui::NextColumn();

            ImGui::PushID(static_cast<int>(i));
            ImGui::Checkbox("##auto", &em.autoFire);
            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
    }
    ImGui::End();
}

// ── Filter Stats Panel ───────────────────────────────────────────────────────

void AudioTestScenario::RenderFilterStatsPanel()
{
    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio Filter Stats"))
    {
        const auto& s = m_audioEngine.GetFilterStats();

        ImGui::Text("Last Frame:");
        ImGui::Text("  Input events:    %u", s.totalIn);
        ImGui::Text("  Passed through:  %u", s.passedOut);
        ImGui::Separator();
        ImGui::Text("Culled breakdown:");
        ImGui::Text("  Distance:   %u", s.culledDistance);
        ImGui::Text("  Cooldown:   %u", s.culledCooldown);
        ImGui::Text("  Dedup:      %u", s.culledDedup);
        ImGui::Text("  Voice Limit:%u", s.culledLimit);

        float passRate = s.totalIn > 0
            ? 100.f * static_cast<float>(s.passedOut) / static_cast<float>(s.totalIn)
            : 0.f;
        ImGui::Separator();
        ImGui::Text("Pass rate: %.1f%%", passRate);

        // Visual bar
        ImGui::ProgressBar(passRate / 100.f, ImVec2(-1, 0), "");
    }
    ImGui::End();
}

// ── Event Log Panel ──────────────────────────────────────────────────────────

void AudioTestScenario::RenderEventLogPanel()
{
    ImGui::SetNextWindowPos(ImVec2(470, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 300), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio Event Log"))
    {
        // Show most recent entries at top.
        ImGui::BeginChild("log_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        // Iterate in reverse order for most-recent-first.
        for (auto it = m_eventLog.rbegin(); it != m_eventLog.rend(); ++it)
            ImGui::TextUnformatted(it->text.c_str());

        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace SagaSandbox
