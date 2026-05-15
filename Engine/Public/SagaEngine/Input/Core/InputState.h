#pragma once

/// @file InputState.h
/// @brief Per-device accumulated input state with edge detection.
///
/// Layer  : Input / Core
/// Purpose: Tracks the current and previous frame state for all input sources.
///          Provides IsDown, WasJustPressed, WasJustReleased, HoldDuration.
///          Consumed by InputActionMap to resolve actions.
///
/// Rules  :
///   - No platform dependencies. No SDL, no Win32.
///   - No gameplay knowledge. No actions, no bindings.
///   - Built by InputDevice::ConsumeFrame() each tick.
///   - Const references are safe to read outside the input thread
///     after InputManager::Update() completes.

#include "SagaEngine/Input/Types/InputTypes.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>

namespace SagaEngine::Input
{

// Key State

class KeyboardState
{
public:
    static constexpr size_t kKeyCount = static_cast<size_t>(KeyCode::COUNT);

    void SetDown(KeyCode key, bool down) noexcept
    {
        const auto idx = static_cast<size_t>(key);
        if (idx < kKeyCount) m_current.set(idx, down);
    }

    /// Snapshot current → previous. Call at the start of each frame.
    void FlipFrame() noexcept
    {
        m_previous = m_current;
    }

    [[nodiscard]] bool IsDown(KeyCode key) const noexcept
    {
        const auto idx = static_cast<size_t>(key);
        return idx < kKeyCount && m_current.test(idx);
    }

    [[nodiscard]] bool WasDown(KeyCode key) const noexcept
    {
        const auto idx = static_cast<size_t>(key);
        return idx < kKeyCount && m_previous.test(idx);
    }

    /// True only on the first frame the key goes down.
    [[nodiscard]] bool WasJustPressed(KeyCode key) const noexcept
    {
        return IsDown(key) && !WasDown(key);
    }

    /// True only on the first frame the key goes up.
    [[nodiscard]] bool WasJustReleased(KeyCode key) const noexcept
    {
        return !IsDown(key) && WasDown(key);
    }

    void Clear() noexcept
    {
        m_current.reset();
        m_previous.reset();
    }

private:
    std::bitset<kKeyCount> m_current;
    std::bitset<kKeyCount> m_previous;
};

// Mouse State

struct MouseState
{
    static constexpr size_t kButtonCount = static_cast<size_t>(MouseButton::COUNT);

    // Position
    int32_t absX = 0, absY = 0;     ///< Current absolute position
    int32_t relX = 0, relY = 0;     ///< Delta accumulated this frame
    float   scrollX = 0.f, scrollY = 0.f; ///< Scroll delta this frame

    // Buttons: current and previous frame
    std::bitset<kButtonCount> buttonsCurrent;
    std::bitset<kButtonCount> buttonsPrevious;

    void SetButtonDown(MouseButton btn, bool down) noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        if (idx < kButtonCount) buttonsCurrent.set(idx, down);
    }

    [[nodiscard]] bool IsButtonDown(MouseButton btn) const noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        return idx < kButtonCount && buttonsCurrent.test(idx);
    }

    [[nodiscard]] bool WasButtonJustPressed(MouseButton btn) const noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        return idx < kButtonCount
            && buttonsCurrent.test(idx)
            && !buttonsPrevious.test(idx);
    }

    [[nodiscard]] bool WasButtonJustReleased(MouseButton btn) const noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        return idx < kButtonCount
            && !buttonsCurrent.test(idx)
            && buttonsPrevious.test(idx);
    }

    void FlipFrame() noexcept
    {
        buttonsPrevious = buttonsCurrent;
        relX = relY = 0;          ///< Relative delta resets every frame
        scrollX = scrollY = 0.f;
    }

    void Clear() noexcept
    {
        buttonsCurrent.reset();
        buttonsPrevious.reset();
        absX = absY = relX = relY = 0;
        scrollX = scrollY = 0.f;
    }
};

// Gamepad State

struct GamepadState
{
    static constexpr size_t kButtonCount = static_cast<size_t>(GamepadButton::COUNT);
    static constexpr size_t kAxisCount   = static_cast<size_t>(GamepadAxis::COUNT);

    DeviceId deviceId = kInvalidDeviceId;
    bool     connected = false;

    std::bitset<kButtonCount> buttonsCurrent;
    std::bitset<kButtonCount> buttonsPrevious;
    std::array<float, kAxisCount> axes{};   ///< [-1, 1] sticks, [0, 1] triggers

    void SetButtonDown(GamepadButton btn, bool down) noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        if (idx < kButtonCount) buttonsCurrent.set(idx, down);
    }

    void SetAxis(GamepadAxis axis, float value) noexcept
    {
        const auto idx = static_cast<size_t>(axis);
        if (idx < kAxisCount) axes[idx] = value;
    }

    [[nodiscard]] bool IsButtonDown(GamepadButton btn) const noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        return idx < kButtonCount && buttonsCurrent.test(idx);
    }

    [[nodiscard]] bool WasButtonJustPressed(GamepadButton btn) const noexcept
    {
        const auto idx = static_cast<size_t>(btn);
        return idx < kButtonCount
            && buttonsCurrent.test(idx)
            && !buttonsPrevious.test(idx);
    }

    [[nodiscard]] float GetAxis(GamepadAxis axis) const noexcept
    {
        const auto idx = static_cast<size_t>(axis);
        return idx < kAxisCount ? axes[idx] : 0.f;
    }

    void FlipFrame() noexcept
    {
        buttonsPrevious = buttonsCurrent;
    }

    void Clear() noexcept
    {
        buttonsCurrent.reset();
        buttonsPrevious.reset();
        axes.fill(0.f);
    }
};

// Composite Device State

/// Aggregated state for a single logical input device.
/// InputDevice holds one of these and exposes it via GetState().

struct InputState
{
    DeviceType    deviceType = DeviceType::Unknown;
    DeviceId      deviceId   = kInvalidDeviceId;
    uint64_t      lastUpdateUs = 0;   ///< Platform timestamp of last event

    KeyboardState keyboard;
    MouseState    mouse;
    GamepadState  gamepad;

    /// Call at the start of each frame before ConsumeFrame().
    void FlipFrame() noexcept
    {
        keyboard.FlipFrame();
        mouse.FlipFrame();
        gamepad.FlipFrame();
    }

    void Clear() noexcept
    {
        keyboard.Clear();
        mouse.Clear();
        gamepad.Clear();
    }
};

} // namespace SagaEngine::Input