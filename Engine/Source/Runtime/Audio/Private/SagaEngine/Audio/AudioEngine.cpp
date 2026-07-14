/// @file AudioEngine.cpp
/// @brief Top-level audio engine — full pipeline: intent → map → filter → dispatch.

#include "SagaEngine/Audio/AudioEngine.h"

namespace SagaEngine::Audio
{

AudioEngine::~AudioEngine()
{
    if (m_initialized)
        Shutdown();
}

// ── Lifecycle ────────────────────────────────────────────────────────

bool AudioEngine::Initialize(const AudioEngineConfig& cfg)
{
    if (m_initialized) return true;

    m_filter.SetConfig(cfg.filterConfig);

    // If no backend was set externally, create a null one.
    if (!m_backend)
        m_backend = std::make_unique<NullAudioBackend>();

    if (!m_backend->Initialize())
        return false;

    // Register built-in mapping rules.
    m_mapper.RegisterDefaults();

    m_initialized = true;
    return true;
}

void AudioEngine::Shutdown()
{
    if (!m_initialized) return;

    m_queue.Clear();
    m_pendingIntents.clear();
    m_frameBatch.Clear();
    m_filter.ResetCooldowns();

    if (m_backend)
        m_backend->Shutdown();

    m_initialized = false;
}

void AudioEngine::SetBackend(std::unique_ptr<IAudioBackend> backend)
{
    m_backend = std::move(backend);
}

// ── Per-frame ────────────────────────────────────────────────────────

void AudioEngine::SetListenerContext(const ListenerContext& ctx)
{
    m_listenerCtx = ctx;

    if (m_backend)
        m_backend->SetListenerTransform(ctx.listenerTransform);
}

void AudioEngine::PostIntent(const GameplayAudioIntent& intent)
{
    m_pendingIntents.push_back(intent);
}

void AudioEngine::Update(float dt)
{
    if (!m_initialized) return;

    float dtMs = dt * 1000.f;
    m_filter.Tick(dtMs);

    // Step 1: Map all pending intents → AudioEvents.
    m_frameBatch.Clear();
    m_mapper.MapAll(m_pendingIntents, m_listenerCtx, m_frameBatch);
    m_pendingIntents.clear();

    // Step 2: Filter (cull, dedup, prioritise).
    m_filter.Filter(m_frameBatch);

    // Step 3: Push surviving events into the thread-safe queue.
    m_queue.PushBatch(m_frameBatch);
}

void AudioEngine::DispatchToBackend()
{
    if (!m_initialized || !m_backend) return;

    std::vector<AudioEvent> events;
    m_queue.Drain(events);

    for (auto& e : events)
    {
        // Set RTPCs before posting.
        for (auto& rtpc : e.rtpcs)
            m_backend->SetRTPC(e.gameObject, rtpc.paramName.c_str(), rtpc.value);

        // Set switches.
        for (auto& sw : e.switches)
            m_backend->SetSwitch(e.gameObject, sw.groupName.c_str(), sw.valueName.c_str());

        // Update position.
        m_backend->SetObjectTransform(e.gameObject, e.transform);

        // Post the event.
        m_backend->PostEvent(e.eventName.c_str(), e.gameObject);
    }

    // Let the backend do its per-frame processing (Wwise: RenderAudio).
    m_backend->RenderAudio();
}

// ── Game object helpers ──────────────────────────────────────────────

void AudioEngine::RegisterObject(AudioObjectId id)
{
    if (m_backend) m_backend->RegisterGameObject(id);
}

void AudioEngine::UnregisterObject(AudioObjectId id)
{
    if (m_backend)
    {
        m_backend->StopAll(id);
        m_backend->UnregisterGameObject(id);
    }
}

void AudioEngine::SetObjectTransform(AudioObjectId id, const AudioTransform& t)
{
    if (m_backend) m_backend->SetObjectTransform(id, t);
}

// ── State / Switch shortcuts ─────────────────────────────────────────

void AudioEngine::SetEnvironment(EnvironmentType env)
{
    if (!m_backend) return;

    const char* stateName = "Outdoor";
    switch (env)
    {
        case EnvironmentType::Indoor:     stateName = "Indoor";     break;
        case EnvironmentType::Cave:       stateName = "Cave";       break;
        case EnvironmentType::Forest:     stateName = "Forest";     break;
        case EnvironmentType::City:       stateName = "City";       break;
        case EnvironmentType::Dungeon:    stateName = "Dungeon";    break;
        case EnvironmentType::Underwater: stateName = "Underwater"; break;
        default: break;
    }
    m_backend->SetState("Environment", stateName);
}

void AudioEngine::SetGlobalRTPC(const char* param, float value)
{
    // Global RTPC — post on the invalid object (Wwise convention).
    if (m_backend)
        m_backend->SetRTPC(AudioObjectId::kInvalid, param, value);
}

// ── Diagnostics ──────────────────────────────────────────────────────

const AudioFilterStats& AudioEngine::GetFilterStats() const noexcept
{
    return m_filter.LastStats();
}

std::size_t AudioEngine::QueueDepth() const noexcept
{
    return m_queue.ApproxSize();
}

} // namespace SagaEngine::Audio
