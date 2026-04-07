/// @file InputProbeScenario.cpp
/// @brief Input pipeline probe scenario implementation.

#include "Scenarios/InputProbeScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Input/Backends/SDL/SDLInputBackend.h>
#include <SagaEngine/Input/Devices/KeyboardDevice.h>
#include <SagaEngine/Input/Devices/MouseDevice.h>
#include <SagaEngine/Input/Commands/InputCommandSerializer.h>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(InputProbeScenario);

static constexpr const char* kTag = "InputProbeScenario";

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool InputProbeScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising input probe…");

    // Attach SDL backend
    auto backend = std::make_unique<SagaEngine::Platform::SDLInputBackend>();
    m_inputManager.SetBackend(std::move(backend));

    if (!m_inputManager.Initialize())
    {
        LOG_ERROR(kTag, "InputManager failed to initialise.");
        return false;
    }

    // Register keyboard and mouse devices
    m_inputManager.RegisterDevice(
        SagaEngine::Input::MakeKeyboardDevice(1));
    m_inputManager.RegisterDevice(
        SagaEngine::Input::MakeMouseDevice(2));

    m_recentKeyEvents.reserve(64);
    LOG_INFO(kTag, "Input probe ready.");
    return true;
}

void InputProbeScenario::OnUpdate(float /*dt*/, uint64_t tick)
{
    m_inputManager.Update(static_cast<uint32_t>(tick));

    const auto& frame = m_inputManager.GetCurrentFrame();

    // Record recent key events
    for (const auto& action : frame.actions)
    {
        (void)action;  // process actions here in a real scenario
    }

    // Build a synthetic InputCommand from this frame and push to buffer
    auto cmd = SagaEngine::Input::MakeInputCommand(
        ++m_sequenceCounter,
        static_cast<uint32_t>(tick),
        0 /* clientTimeUs — not available without platform timer here */
    );

    cmd.moveX = SagaEngine::Input::FixedFromFloat(frame.move.x);
    cmd.moveY = SagaEngine::Input::FixedFromFloat(frame.move.y);
    cmd.lookX = SagaEngine::Input::FixedFromFloat(frame.look.x);
    cmd.lookY = SagaEngine::Input::FixedFromFloat(frame.look.y);

    m_commandBuffer.PushLocal(cmd);
}

void InputProbeScenario::OnShutdown()
{
    m_inputManager.Shutdown();
    m_commandBuffer.Reset();
    m_recentKeyEvents.clear();
    LOG_INFO(kTag, "Input probe shut down.");
}

// ─── Debug UI ─────────────────────────────────────────────────────────────────

void InputProbeScenario::OnRenderDebugUI()
{
    RenderRawInputPanel();
    RenderActionMapPanel();
    RenderCommandBufferPanel();
}

void InputProbeScenario::RenderRawInputPanel()
{
    // ImGui::Begin("Raw Input State");
    // Render keyboard bitset, mouse position/delta, scroll, gamepad axes
    // ...
    // ImGui::End();
}

void InputProbeScenario::RenderActionMapPanel()
{
    // ImGui::Begin("Action Map");
    // List resolved actions from GetCurrentFrame().actions
    // ...
    // ImGui::End();
}

void InputProbeScenario::RenderCommandBufferPanel()
{
    // ImGui::Begin("InputCommandBuffer");
    // Show pending count, unacked count, serialised wire bytes
    // ...
    // ImGui::End();
}

} // namespace SagaSandbox