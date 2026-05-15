/// @file GamepadDevice.cpp

#include "SagaEngine/Input/Devices/GamepadDevice.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"

namespace SagaEngine::Input
{

GamepadDevice::GamepadDevice(DeviceId id)
    : m_id(id)
{
    m_state.deviceType       = DeviceType::Gamepad;
    m_state.deviceId         = id;
    m_state.gamepad.deviceId = id;
    m_state.gamepad.connected= false;
}

DeviceType       GamepadDevice::GetType()     const noexcept { return DeviceType::Gamepad; }
DeviceId         GamepadDevice::GetId()       const noexcept { return m_id; }
std::string_view GamepadDevice::GetName()     const noexcept { return "Gamepad"; }
bool             GamepadDevice::IsConnected() const noexcept { return m_state.gamepad.connected; }

void GamepadDevice::BeginFrame() noexcept
{
    m_state.FlipFrame();
}

void GamepadDevice::ConsumeFrame(const RawInputFrame& frame)
{
    m_state.lastUpdateUs = frame.frameTimestampUs;

    // ── Connection events ─────────────────────────────────────────────────────
    // Process first: if a gamepad disconnects this frame, ignore its other events.
    for (const auto& ev : frame.gamepadConnections)
    {
        if (ev.deviceId != m_id) continue;
        m_state.gamepad.connected = ev.connected;
        if (!ev.connected)
            m_state.gamepad.Clear();  // zero-out state on disconnect
    }

    if (!m_state.gamepad.connected) return;

    // ── Buttons ───────────────────────────────────────────────────────────────
    for (const auto& ev : frame.gamepadButtons)
    {
        if (ev.deviceId != m_id) continue;
        m_state.gamepad.SetButtonDown(ev.button, ev.pressed);
    }

    // ── Axes ──────────────────────────────────────────────────────────────────
    for (const auto& ev : frame.gamepadAxes)
    {
        if (ev.deviceId != m_id) continue;
        m_state.gamepad.SetAxis(ev.axis, ev.value);
    }
}

const InputState& GamepadDevice::GetState() const noexcept
{
    return m_state;
}

// ─── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<GamepadDevice> MakeGamepadDevice(DeviceId id)
{
    return std::make_unique<GamepadDevice>(id);
}

} // namespace SagaEngine::Input