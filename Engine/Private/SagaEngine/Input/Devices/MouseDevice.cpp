/// @file MouseDevice.cpp

#include "SagaEngine/Input/Devices/MouseDevice.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"

namespace SagaEngine::Input
{

MouseDevice::MouseDevice(DeviceId id)
    : m_id(id)
{
    m_state.deviceType = DeviceType::Mouse;
    m_state.deviceId   = id;
}

DeviceType       MouseDevice::GetType() const noexcept { return DeviceType::Mouse; }
DeviceId         MouseDevice::GetId()   const noexcept { return m_id; }
std::string_view MouseDevice::GetName() const noexcept { return "Mouse"; }

void MouseDevice::BeginFrame() noexcept
{
    // FlipFrame() resets relX/relY and scrollX/Y to 0, flips button state.
    m_state.FlipFrame();
}

void MouseDevice::ConsumeFrame(const RawInputFrame& frame)
{
    m_state.lastUpdateUs = frame.frameTimestampUs;

    // ── Movement ─────────────────────────────────────────────────────────────
    // High-polling-rate mice send multiple move events per frame.
    // Accumulate relative deltas; use the last absolute position.
    for (const auto& ev : frame.mouseMoves)
    {
        m_state.mouse.relX += ev.relX;
        m_state.mouse.relY += ev.relY;
        m_state.mouse.absX  = ev.absX;   // last position wins
        m_state.mouse.absY  = ev.absY;
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    for (const auto& ev : frame.mouseButtons)
        m_state.mouse.SetButtonDown(ev.button, ev.pressed);

    // ── Scroll ────────────────────────────────────────────────────────────────
    // Accumulate: trackpad may deliver many small precise events per frame.
    for (const auto& ev : frame.mouseScrolls)
    {
        m_state.mouse.scrollX += ev.deltaX;
        m_state.mouse.scrollY += ev.deltaY;
    }
}

const InputState& MouseDevice::GetState() const noexcept
{
    return m_state;
}

// ─── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<MouseDevice> MakeMouseDevice(DeviceId id)
{
    return std::make_unique<MouseDevice>(id);
}

} // namespace SagaEngine::Input