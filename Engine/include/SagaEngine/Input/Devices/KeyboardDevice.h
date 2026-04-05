#pragma once

/// @file KeyboardDevice.h
/// @brief Concrete keyboard implementation of InputDevice.
///
/// Layer  : Input / Devices
/// Purpose: Consumes RawKeyEvents from a RawInputFrame and maintains
///          a full KeyboardState with edge detection (JustPressed/Released).
///          Also accumulates active ModifierFlags each frame.

#include "SagaEngine/Input/Devices/InputDevice.h"
#include "SagaEngine/Input/Core/InputState.h"

namespace SagaEngine::Input
{

class KeyboardDevice final : public InputDevice
{
public:
    explicit KeyboardDevice(DeviceId id);
    ~KeyboardDevice() override = default;

    // ── InputDevice interface ─────────────────────────────────────────────────
    [[nodiscard]] DeviceType       GetType()  const noexcept override;
    [[nodiscard]] DeviceId         GetId()    const noexcept override;
    [[nodiscard]] std::string_view GetName()  const noexcept override;

    void BeginFrame() noexcept override;
    void ConsumeFrame(const RawInputFrame& frame) override;

    [[nodiscard]] const InputState& GetState() const noexcept override;

    // ── Keyboard-specific ─────────────────────────────────────────────────────

    /// Snapshot of active modifier keys this frame.
    [[nodiscard]] ModifierFlags GetActiveModifiers() const noexcept
    {
        return m_activeModifiers;
    }

    /// Accumulated text input events this frame (empty when text mode off).
    [[nodiscard]] const std::vector<RawTextEvent>& GetTextEvents() const noexcept
    {
        return m_textEvents;
    }

private:
    DeviceId        m_id;
    InputState      m_state;
    ModifierFlags   m_activeModifiers = ModifierFlags::None;

    std::vector<RawTextEvent> m_textEvents;
};

} // namespace SagaEngine::Input