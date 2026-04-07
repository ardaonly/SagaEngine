/// @file PredictionStressScenario.h
/// @brief Client-side prediction stress test with configurable divergence injection.
///
/// Layer  : Sandbox / Scenarios
/// Purpose: Runs a local prediction loop and periodically injects artificial
///          server corrections to measure rollback cost and reconcile latency.

#pragma once

#include "SagaSandbox/Core/IScenario.h"
#include <SagaEngine/Input/Networking/ClientPrediction.h>
#include <SagaEngine/Input/Commands/InputCommandBuffer.h>
#include <array>
#include <cstdint>

namespace SagaSandbox
{

// ─── Minimal world state for this scenario ────────────────────────────────────

struct SbSimpleWorldState
{
    float posX = 0.f;
    float posY = 0.f;
    float velX = 0.f;
    float velY = 0.f;
};

// ─── Scenario ─────────────────────────────────────────────────────────────────

class PredictionStressScenario final : public IScenario
{
public:
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "prediction_stress",
        .displayName = "Prediction Stress",
        .category    = "Network",
        .description = "Client-side prediction rollback stress with injected server divergence.",
    };

    PredictionStressScenario()  = default;
    ~PredictionStressScenario() override = default;

    [[nodiscard]] const ScenarioDescriptor& GetDescriptor() const noexcept override
    { return kDescriptor; }

    [[nodiscard]] bool OnInit()                          override;
    void               OnUpdate(float dt, uint64_t tick) override;
    void               OnShutdown()                      override;
    void               OnRenderDebugUI()                 override;

private:
    // ── Simulation helpers ────────────────────────────────────────────────────

    static SbSimpleWorldState SimulateOneTick(const SbSimpleWorldState& state,
                                              const SagaEngine::Input::InputCommand& cmd);

    static bool StatesEqual(const SbSimpleWorldState& a, const SbSimpleWorldState& b);

    void InjectServerCorrection(uint32_t atTick);

    // ── State ─────────────────────────────────────────────────────────────────

    using Prediction = SagaEngine::Input::ClientPrediction<SbSimpleWorldState>;
    std::unique_ptr<Prediction> m_prediction;

    SagaEngine::Input::InputCommandBuffer m_cmdBuffer;
    SbSimpleWorldState                    m_currentState;

    uint32_t m_localSequence = 0;

    // HUD config
    int   m_divergeEveryNTicks = 60;   ///< Inject server correction every N ticks
    float m_divergeMagnitude   = 5.f;  ///< Position error magnitude in units
    bool  m_enableDivergence   = true;

    // Stats
    uint64_t m_totalRollbacks    = 0;
    uint64_t m_totalCorrections  = 0;
    float    m_lastRollbackMs    = 0.f;

    static constexpr int kHistorySize = 128;
    std::array<float, kHistorySize> m_rollbackHistory{};
    int                             m_historyOffset = 0;
};

} // namespace SagaSandbox