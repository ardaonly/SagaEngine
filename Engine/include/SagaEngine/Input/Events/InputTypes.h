#pragma once
#include <cstdint>

namespace Saga::Input {
using DeviceId = uint32_t;
using Timestamp = uint64_t;

enum class InputEventType : uint8_t {
    KeyDown,
    KeyUp,
    KeyRepeat,
    MouseMove,
    MouseButtonDown,
    MouseButtonUp,
    GamepadButtonDown,
    GamepadButtonUp,
    Axis
};

enum class MouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2
};
}
