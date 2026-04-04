#pragma once

/// @file InputDevice.h
/// @brief Logical input device interface: translates raw events → InputState.
///
/// Layer  : Input / Devices
/// Purpose: One InputDevice per physical device category (keyboard, mouse, gamepad).
///          Consumes a RawInputFrame (from backend), filters events relevant
///          to its device type, updates its InputState (current + edge detection).
///
/// Hierarchy vs IInputDevice.h:
///   Old IInputDevice: polled one event at a time, tracked state itself,
///   exposed IsKeyDown() — mixing platform and state responsibilities.
///
///   This InputDevice: receives a full RawInputFrame each tick, extracts
///   only what it cares about, and owns an InputState. No platform knowledge.
///
/// Rules  :
///   - No platform headers included here.
///   - No action/mapping knowledge.
///   - No network serialization.
///   - No gameplay code.
///   - Implementations (KeyboardDevice, MouseDevice, GamepadDevice)
///     live in Engine/src/Input/Devices/.

#include "SagaEngine/Input/Core/InputState.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"
#include <memory>
#include <string_view>

namespace SagaEngine::Input
{

class InputDevice
{
public:
    virtual ~InputDevice() = default;

    // Identity

    [[nodiscard]] virtual DeviceType      GetType()     const noexcept = 0;
    [[nodiscard]] virtual DeviceId        GetId()       const noexcept = 0;
    [[nodiscard]] virtual std::string_view GetName()    const noexcept = 0;

    // Per-frame lifecycle

    /// Advance state: snapshot current → previous, clear per-frame deltas.
    /// Must be called BEFORE ConsumeFrame() each tick.
    virtual void BeginFrame() noexcept = 0;

    /// Consume relevant events from the frame and update internal InputState.
    /// Implementations filter events by device type and/or deviceId.
    virtual void ConsumeFrame(const RawInputFrame& frame) = 0;

    // State access

    /// Returns the fully updated state for this frame.
    /// Valid to read after ConsumeFrame() returns.
    [[nodiscard]] virtual const InputState& GetState() const noexcept = 0;

    // Connection

    [[nodiscard]] virtual bool IsConnected() const noexcept { return true; }

    // Non-copyable
    InputDevice(const InputDevice&) = delete;
    InputDevice& operator=(const InputDevice&) = delete;

protected:
    InputDevice() = default;
};

// Concrete device forward declarations
// Implementations live in Engine/src/Input/Devices/

class KeyboardDevice;
class MouseDevice;
class GamepadDevice;

// Factory helpers

[[nodiscard]] std::unique_ptr<KeyboardDevice> MakeKeyboardDevice(DeviceId id);
[[nodiscard]] std::unique_ptr<MouseDevice>    MakeMouseDevice(DeviceId id);
[[nodiscard]] std::unique_ptr<GamepadDevice>  MakeGamepadDevice(DeviceId id);

} // namespace SagaEngine::Input