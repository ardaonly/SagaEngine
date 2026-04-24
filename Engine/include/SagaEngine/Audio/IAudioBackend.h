/// @file IAudioBackend.h
/// @brief Abstract audio backend interface — Wwise (or FMOD / custom) sits behind this.
///
/// The engine never calls Wwise directly.  All interaction goes through
/// IAudioBackend so the audio middleware can be swapped without touching
/// gameplay or mapper code.
///
/// Concrete implementation: WwiseBackend (future), NullAudioBackend (testing).

#pragma once

#include "SagaEngine/Audio/AudioTypes.h"
#include "SagaEngine/Audio/AudioEvent.h"

#include <cstdint>

namespace SagaEngine::Audio
{

class IAudioBackend
{
public:
    virtual ~IAudioBackend() = default;

    // ── Lifecycle ────────────────────────────────────────────────

    [[nodiscard]] virtual bool Initialize() = 0;
    virtual void               Shutdown()   = 0;

    /// Call once per frame — lets the backend do internal housekeeping
    /// (Wwise: AK::SoundEngine::RenderAudio).
    virtual void RenderAudio() = 0;

    // ── Game object management ───────────────────────────────────

    virtual void RegisterGameObject(AudioObjectId id)   = 0;
    virtual void UnregisterGameObject(AudioObjectId id) = 0;
    virtual void SetObjectTransform(AudioObjectId id, const AudioTransform& t) = 0;

    // ── Listener ─────────────────────────────────────────────────

    virtual void SetListenerTransform(const AudioTransform& t) = 0;

    // ── Event posting ────────────────────────────────────────────

    /// Post a named event on a game object.  Returns a play ID.
    [[nodiscard]] virtual AudioPlayId PostEvent(const char* eventName,
                                                 AudioObjectId obj) = 0;

    virtual void StopEvent(AudioPlayId id) = 0;
    virtual void StopAll(AudioObjectId obj) = 0;

    // ── RTPC / Switch / State ────────────────────────────────────

    virtual void SetRTPC(AudioObjectId obj, const char* param, float value) = 0;
    virtual void SetSwitch(AudioObjectId obj, const char* group, const char* value) = 0;
    virtual void SetState(const char* group, const char* state) = 0;

    // ── Bank / package management ────────────────────────────────

    [[nodiscard]] virtual bool LoadBank(const char* bankName) = 0;
    virtual void               UnloadBank(const char* bankName) = 0;
};

// ── Null backend (silent, for tests / headless server) ───────────────

class NullAudioBackend final : public IAudioBackend
{
public:
    bool Initialize() override { return true; }
    void Shutdown()   override {}
    void RenderAudio() override {}

    void RegisterGameObject(AudioObjectId)   override {}
    void UnregisterGameObject(AudioObjectId) override {}
    void SetObjectTransform(AudioObjectId, const AudioTransform&) override {}

    void SetListenerTransform(const AudioTransform&) override {}

    AudioPlayId PostEvent(const char*, AudioObjectId) override
    { return AudioPlayId::kInvalid; }
    void StopEvent(AudioPlayId) override {}
    void StopAll(AudioObjectId) override {}

    void SetRTPC(AudioObjectId, const char*, float) override {}
    void SetSwitch(AudioObjectId, const char*, const char*) override {}
    void SetState(const char*, const char*) override {}

    bool LoadBank(const char*) override { return true; }
    void UnloadBank(const char*) override {}
};

} // namespace SagaEngine::Audio
