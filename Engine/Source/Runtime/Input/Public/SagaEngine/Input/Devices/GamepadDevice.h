#pragma once

/// @file GamepadDevice.h
/// @brief Concrete gamepad implementation of InputDevice.
///
/// Layer  : Input / Devices
/// Purpose: Consumes RawGamepadButtonEvents, RawGamepadAxisEvents, and
///          RawGamepadConnectionEvents for a specific DeviceId.
///          Tracks connection state and reports IsConnected().

#include "SagaEngine/Input/Devices/InputDevice.h"
#include "SagaEngine/Input/Core/InputState.h"

namespace SagaEngine::Input
{

class GamepadDevice final : public InputDevice
{
public:
    explicit GamepadDevice(DeviceId id);
    ~GamepadDevice() override = default;

    // ── InputDevice interface ─────────────────────────────────────────────────
    [[nodiscard]] DeviceType       GetType()      const noexcept override;
    [[nodiscard]] DeviceId         GetId()        const noexcept override;
    [[nodiscard]] std::string_view GetName()      const noexcept override;
    [[nodiscard]] bool             IsConnected()  const noexcept override;

    void BeginFrame() noexcept override;
    void ConsumeFrame(const RawInputFrame& frame) override;

    [[nodiscard]] const InputState& GetState() const noexcept override;

private:
    DeviceId   m_id;
    InputState m_state;
};

} // namespace SagaEngine::Input