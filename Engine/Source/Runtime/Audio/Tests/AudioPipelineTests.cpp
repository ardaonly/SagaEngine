// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Audio/AudioEngine.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Audio {
namespace {

struct BackendState
{
    int initializeCalls = 0;
    int shutdownCalls = 0;
    int renderCalls = 0;
    std::vector<std::string> postedEvents;
    std::vector<std::pair<std::string, float>> rtpcs;
    std::vector<std::pair<std::string, std::string>> switches;
};

class RecordingBackend final : public IAudioBackend
{
public:
    explicit RecordingBackend(BackendState& state) : m_state(state) {}

    bool Initialize() override { ++m_state.initializeCalls; return true; }
    void Shutdown() override { ++m_state.shutdownCalls; }
    void RenderAudio() override { ++m_state.renderCalls; }
    void RegisterGameObject(AudioObjectId) override {}
    void UnregisterGameObject(AudioObjectId) override {}
    void SetObjectTransform(AudioObjectId, const AudioTransform&) override {}
    void SetListenerTransform(const AudioTransform&) override {}
    AudioPlayId PostEvent(const char* name, AudioObjectId) override
    {
        m_state.postedEvents.emplace_back(name);
        return static_cast<AudioPlayId>(m_state.postedEvents.size());
    }
    void StopEvent(AudioPlayId) override {}
    void StopAll(AudioObjectId) override {}
    void SetRTPC(AudioObjectId, const char* name, float value) override
    {
        m_state.rtpcs.emplace_back(name, value);
    }
    void SetSwitch(AudioObjectId, const char* group, const char* value) override
    {
        m_state.switches.emplace_back(group, value);
    }
    void SetState(const char*, const char*) override {}
    bool LoadBank(const char*) override { return true; }
    void UnloadBank(const char*) override {}

private:
    BackendState& m_state;
};

TEST(AudioPipelineTests, MapsFiltersAndDispatchesAnIntentExactlyOnce)
{
    BackendState state;
    AudioEngine engine;
    engine.SetBackend(std::make_unique<RecordingBackend>(state));

    ASSERT_TRUE(engine.Initialize());
    EXPECT_EQ(state.initializeCalls, 1);

    GameplayAudioIntent intent;
    intent.type = "WeaponFired";
    intent.sourceObject = static_cast<AudioObjectId>(42);
    intent.weaponId = 7;

    engine.PostIntent(intent);
    engine.PostIntent(intent);
    engine.Update(1.0f / 60.0f);

    EXPECT_EQ(engine.GetFilterStats().totalIn, 2u);
    EXPECT_EQ(engine.GetFilterStats().passedOut, 1u);
    EXPECT_EQ(engine.QueueDepth(), 1u);

    engine.DispatchToBackend();
    EXPECT_EQ(engine.QueueDepth(), 0u);
    ASSERT_EQ(state.postedEvents.size(), 1u);
    EXPECT_EQ(state.postedEvents.front(), "Play_Weapon_Fire_1P");
    EXPECT_EQ(state.renderCalls, 1);
    EXPECT_FALSE(state.rtpcs.empty());
    ASSERT_EQ(state.switches.size(), 1u);
    EXPECT_EQ(state.switches.front(), std::make_pair(std::string("WeaponId"), std::string("7")));

    engine.Shutdown();
    EXPECT_EQ(state.shutdownCalls, 1);
}

TEST(AudioPipelineTests, DistanceFilterKeepsCriticalEventsOnly)
{
    AudioFilterConfig config;
    config.cullDistance = 10.0f;
    AudioFilter filter(config);
    AudioEventBatch batch;

    AudioEvent ordinary;
    ordinary.distance = 11.0f;
    ordinary.priority = AudioPriority::Normal;
    batch.Push(ordinary);

    AudioEvent critical;
    critical.distance = 1000.0f;
    critical.priority = AudioPriority::Critical;
    batch.Push(critical);

    const auto stats = filter.Filter(batch);
    EXPECT_EQ(stats.totalIn, 2u);
    EXPECT_EQ(stats.culledDistance, 1u);
    ASSERT_EQ(batch.Size(), 1u);
    EXPECT_EQ(batch.events.front().priority, AudioPriority::Critical);
}

} // namespace
} // namespace SagaEngine::Audio
