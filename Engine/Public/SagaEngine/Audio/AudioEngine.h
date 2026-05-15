/// @file AudioEngine.h
/// @brief Top-level audio engine — orchestrates the full pipeline each frame.
///
/// Pipeline per frame:
///   1. Gameplay pushes GameplayAudioIntents.
///   2. AudioEventMapper produces AudioEvents (context enrichment).
///   3. AudioFilter culls / deduplicates / prioritises.
///   4. AudioQueue bridges to the audio thread.
///   5. Audio thread drains queue → IAudioBackend (Wwise).
///
/// The server sends ONLY intents — never "play sound X".

#pragma once

#include "SagaEngine/Audio/AudioTypes.h"
#include "SagaEngine/Audio/AudioEvent.h"
#include "SagaEngine/Audio/AudioEventMapper.h"
#include "SagaEngine/Audio/AudioFilter.h"
#include "SagaEngine/Audio/AudioQueue.h"
#include "SagaEngine/Audio/IAudioBackend.h"

#include <memory>
#include <vector>

namespace SagaEngine::Audio
{

/// Configuration for the audio engine.
struct AudioEngineConfig
{
    AudioFilterConfig filterConfig{};
    bool              useNullBackend = false;   ///< true = silent (headless / tests).
};

class AudioEngine
{
public:
    AudioEngine() = default;
    ~AudioEngine();

    // ── Lifecycle ────────────────────────────────────────────────

    [[nodiscard]] bool Initialize(const AudioEngineConfig& cfg = {});
    void               Shutdown();

    /// Attach a custom backend.  Takes ownership.  If not called before
    /// Initialize(), a NullAudioBackend is created.
    void SetBackend(std::unique_ptr<IAudioBackend> backend);

    // ── Per-frame ────────────────────────────────────────────────

    /// Update listener position + environment (call from gameplay thread).
    void SetListenerContext(const ListenerContext& ctx);

    /// Submit a gameplay intent — will be mapped + filtered this frame.
    void PostIntent(const GameplayAudioIntent& intent);

    /// Process the frame: map → filter → enqueue → dispatch.
    /// Call once per frame from the gameplay thread.
    void Update(float dt);

    /// Dispatch queued events to the backend.
    /// If running single-threaded, call right after Update().
    /// If multi-threaded, call from the audio thread.
    void DispatchToBackend();

    // ── Game object helpers ──────────────────────────────────────

    void RegisterObject(AudioObjectId id);
    void UnregisterObject(AudioObjectId id);
    void SetObjectTransform(AudioObjectId id, const AudioTransform& t);

    // ── State / Switch shortcuts ─────────────────────────────────

    void SetEnvironment(EnvironmentType env);
    void SetGlobalRTPC(const char* param, float value);

    // ── Diagnostics ──────────────────────────────────────────────

    [[nodiscard]] const AudioFilterStats& GetFilterStats() const noexcept;
    [[nodiscard]] std::size_t             QueueDepth()     const noexcept;
    [[nodiscard]] IAudioBackend*          GetBackend()     const noexcept { return m_backend.get(); }

    // ── Mapper access (for registering custom rules) ─────────────

    [[nodiscard]] AudioEventMapper&       GetMapper() noexcept { return m_mapper; }

private:
    std::unique_ptr<IAudioBackend> m_backend;
    AudioEventMapper               m_mapper;
    AudioFilter                    m_filter;
    AudioQueue                     m_queue;
    ListenerContext                m_listenerCtx{};

    std::vector<GameplayAudioIntent> m_pendingIntents;
    AudioEventBatch                  m_frameBatch;

    bool m_initialized = false;
};

} // namespace SagaEngine::Audio
