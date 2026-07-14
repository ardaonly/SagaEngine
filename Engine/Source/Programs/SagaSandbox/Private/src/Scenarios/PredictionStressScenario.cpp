/// @file PredictionStressScenario.cpp

#include "Scenarios/PredictionStressScenario.h"
#include <SagaSandbox/Core/ScenarioRegistry.h>
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Input/Commands/InputCommand.h>
#include <imgui.h>
#include <chrono>
#include <cmath>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(PredictionStressScenario);

static constexpr const char* kTag = "PredictionStress";

// ─── Static helpers ───────────────────────────────────────────────────────────

SbSimpleWorldState PredictionStressScenario::SimulateOneTick(
    const SbSimpleWorldState& state,
    const SagaEngine::Input::InputCommand& cmd)
{
    SbSimpleWorldState next = state;

    // Simple physics: input moves velocity, velocity moves position
    const float moveX = SagaEngine::Input::FloatFromFixed(cmd.moveX);
    const float moveY = SagaEngine::Input::FloatFromFixed(cmd.moveY);

    constexpr float kDt          = 1.f / 60.f;
    constexpr float kAcceleration = 20.f;
    constexpr float kDamping      = 0.85f;

    next.velX = (next.velX + moveX * kAcceleration * kDt) * kDamping;
    next.velY = (next.velY + moveY * kAcceleration * kDt) * kDamping;
    next.posX += next.velX * kDt;
    next.posY += next.velY * kDt;

    return next;
}

bool PredictionStressScenario::StatesEqual(
    const SbSimpleWorldState& a, const SbSimpleWorldState& b)
{
    constexpr float kThreshold = 0.01f;
    const float dx = a.posX - b.posX;
    const float dy = a.posY - b.posY;
    return (dx * dx + dy * dy) < (kThreshold * kThreshold);
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool PredictionStressScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising prediction stress…");

    using Prediction = SagaEngine::Input::ClientPrediction<SbSimpleWorldState>;

    Prediction::Config cfg;
    cfg.simulate    = &SimulateOneTick;
    cfg.statesEqual = &StatesEqual;

    m_prediction = std::make_unique<Prediction>(std::move(cfg));
    m_currentState = {};

    LOG_INFO(kTag, "Prediction stress ready.");
    return true;
}

void PredictionStressScenario::OnUpdate(float /*dt*/, uint64_t tick)
{
    // ── Build a synthetic input command ───────────────────────────────────────
    auto cmd = SagaEngine::Input::MakeInputCommand(
        ++m_localSequence,
        static_cast<uint32_t>(tick),
        0
    );
    // Circular motion input
    cmd.moveX = SagaEngine::Input::FixedFromFloat(std::cos(tick * 0.05f));
    cmd.moveY = SagaEngine::Input::FixedFromFloat(std::sin(tick * 0.05f));

    m_cmdBuffer.PushLocal(cmd);

    // ── Apply local prediction ────────────────────────────────────────────────
    m_prediction->ApplyLocalCommand(cmd, m_currentState);
    m_currentState = m_prediction->GetPredictedState();

    // ── Simulate server correction every N ticks ──────────────────────────────
    if (m_enableDivergence && (tick > 0) &&
        (tick % static_cast<uint64_t>(m_divergeEveryNTicks) == 0))
    {
        InjectServerCorrection(static_cast<uint32_t>(tick));
    }

    // ── Ack old commands ──────────────────────────────────────────────────────
    // Keep only the last 32 commands in the buffer (simulate server ack)
    if (m_localSequence > 32)
        m_cmdBuffer.AckUpTo(m_localSequence - 32);
}

void PredictionStressScenario::InjectServerCorrection(uint32_t atTick)
{
    // Authoritative state with artificial error
    SbSimpleWorldState authState = m_currentState;
    authState.posX += m_divergeMagnitude;
    authState.posY += m_divergeMagnitude;

    ++m_totalCorrections;

    const auto unacked = m_cmdBuffer.GetUnackedSnapshot();

    const auto t0 = std::chrono::steady_clock::now();

    auto result = m_prediction->Reconcile(
        atTick - 1,
        authState,
        std::span<const SagaEngine::Input::InputCommand>(unacked.data(), unacked.size())
    );

    const auto t1 = std::chrono::steady_clock::now();
    m_lastRollbackMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (result.corrected)
    {
        ++m_totalRollbacks;
        m_currentState = result.correctedState;
        LOG_DEBUG(kTag, "Rollback #%llu at tick %u (%.3f ms)",
                  (unsigned long long)m_totalRollbacks, atTick, m_lastRollbackMs);
    }

    // History
    m_rollbackHistory[m_historyOffset] = m_lastRollbackMs;
    m_historyOffset = (m_historyOffset + 1) % kHistorySize;
}

void PredictionStressScenario::OnShutdown()
{
    m_prediction.reset();
    m_cmdBuffer.Reset();
    LOG_INFO(kTag, "Prediction stress shut down. Rollbacks=%llu Corrections=%llu",
             (unsigned long long)m_totalRollbacks,
             (unsigned long long)m_totalCorrections);
}

// ─── Debug UI ─────────────────────────────────────────────────────────────────

void PredictionStressScenario::OnRenderDebugUI()
{
    if (!ImGui::Begin("Prediction Stress"))
    {
        ImGui::End();
        return;
    }

    // ── Config ────────────────────────────────────────────────────────────────

    ImGui::SeparatorText("Config");
    ImGui::Checkbox("Enable divergence injection", &m_enableDivergence);
    ImGui::SliderInt("Inject every N ticks",  &m_divergeEveryNTicks, 10, 300);
    ImGui::SliderFloat("Diverge magnitude",   &m_divergeMagnitude, 0.1f, 50.f);

    // ── State ─────────────────────────────────────────────────────────────────

    ImGui::SeparatorText("Predicted State");
    const auto& s = m_currentState;
    ImGui::Text("pos  : (%.2f, %.2f)", s.posX, s.posY);
    ImGui::Text("vel  : (%.2f, %.2f)", s.velX, s.velY);
    ImGui::Text("Tick : %u",  m_prediction->GetLatestPredictedTick());

    // ── Stats ─────────────────────────────────────────────────────────────────

    ImGui::SeparatorText("Rollback Stats");

    char overlay[48];
    std::snprintf(overlay, sizeof(overlay), "last %.3f ms", m_lastRollbackMs);
    ImGui::PlotLines("##rb", m_rollbackHistory.data(), kHistorySize,
                     m_historyOffset, overlay, 0.f, 2.f, ImVec2(0, 50));

    ImGui::Text("Total rollbacks    : %llu", (unsigned long long)m_totalRollbacks);
    ImGui::Text("Total corrections  : %llu", (unsigned long long)m_totalCorrections);
    ImGui::Text("Last rollback cost : %.4f ms", m_lastRollbackMs);

    ImGui::Text("Cmd buffer unacked : %zu", m_cmdBuffer.GetUnackedCount());
    ImGui::Text("Cmd buffer pending : %zu", m_cmdBuffer.GetPendingCount());

    ImGui::End();
}

} // namespace SagaSandbox