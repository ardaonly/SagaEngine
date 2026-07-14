/// @file InputProbeScenario.h
/// @brief Scenario that probes and visualises the full input pipeline.
///
/// Layer  : Sandbox / Scenarios / Catalog
/// Purpose: Tests the entire input chain from platform backend through
///          DeviceRegistry → InputManager → GameplayInputFrame → InputCommand.
///          Displays raw device state, mapped actions, and serialised commands.
///
/// What this scenario exercises:
///   - SDLInputBackend polling
///   - KeyboardDevice / MouseDevice / GamepadDevice state
///   - InputActionMap resolution
///   - InputCommandBuffer push and ack cycle
///   - InputCommandSerializer wire format validation

#pragma once

#include "SagaSandbox/Core/IScenario.h"
#include <SagaEngine/Input/Core/InputManager.h>
#include <SagaEngine/Input/Commands/InputCommandBuffer.h>
#include <memory>
#include <vector>

namespace SagaSandbox
{

class InputProbeScenario final : public IScenario
{
public:
    /// Static descriptor — required by SAGA_SANDBOX_REGISTER_SCENARIO.
    static constexpr ScenarioDescriptor kDescriptor
    {
        .id          = "input_probe",
        .displayName = "Input Probe",
        .category    = "Input",
        .description = "Exercises the full input pipeline from SDL backend to InputCommand wire format.",
    };

    InputProbeScenario()  = default;
    ~InputProbeScenario() override = default;

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
    // ── Internal helpers ──────────────────────────────────────────────────────

    void RenderRawInputPanel();
    void RenderActionMapPanel();
    void RenderCommandBufferPanel();

    // ── State ─────────────────────────────────────────────────────────────────

    SagaEngine::Input::InputManager        m_inputManager;
    SagaEngine::Input::InputCommandBuffer  m_commandBuffer;

    uint32_t                               m_sequenceCounter = 0;

    // Ring buffer of last N keyboard events for display
    struct KeyEventRecord
    {
        SagaEngine::Input::KeyCode key;
        bool                       pressed;
        uint64_t                   timestampUs;
    };
    std::vector<KeyEventRecord> m_recentKeyEvents;  ///< Capped at 64 entries
};

} // namespace SagaSandbox