// include/SagaEngine/NativePlatform/IInputDevice.h
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace SagaEngine::NativePlatform
{

enum class KeyCode : uint32_t
{
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Escape, LCtrl, RCtrl, LShift, RShift, LAlt, RAlt,
    Space, Enter, Backspace, Tab,
    Left, Right, Up, Down,
    // add mouse buttons & gamepad buttons later as separate enums/variants
};

struct KeyEvent
{
    KeyCode code;
    bool pressed;
    uint64_t timestamp;
};

class IInputDevice
{
public:
    virtual ~IInputDevice() = default;

    virtual void Update() = 0;

    virtual std::optional<KeyEvent> PollEvent() = 0;

    virtual bool IsKeyDown(KeyCode key) const = 0;

    virtual std::optional<std::string> PollTextInput() = 0;
};
}
