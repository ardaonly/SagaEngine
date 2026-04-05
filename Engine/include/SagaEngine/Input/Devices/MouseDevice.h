#pragma once

/// @file MouseDevice.h
/// @brief Concrete mouse implementation of InputDevice.
///
/// Layer  : Input / Devices
/// Purpose: Consumes RawMouseMoveEvents, RawMouseButtonEvents, and
///          RawMouseScrollEvents from a RawInputFrame.
///          Accumulates relative delta across multiple events in a single frame
///          (common with high-polling-rate mice), maintains button state
///          with edge detection, and aggregates scroll.

#include "SagaEngine/Input/Devices/InputDevice.h"
#include "SagaEngine/Input/Core/InputState.h"

namespace SagaEngine::Input
{

class MouseDevice final : public InputDevice
{
public:
    explicit MouseDevice(DeviceId id);
    ~MouseDevice() override = default;

    // ── InputDevice interface ─────────────────────────────────────────────────
    [[nodiscard]] DeviceType       GetType() const noexcept override;
    [[nodiscard]] DeviceId         GetId()   const noexcept override;
    [[nodiscard]] std::string_view GetName() const noexcept override;

    void BeginFrame() noexcept override;
    void ConsumeFrame(const RawInputFrame& frame) override;

    [[nodiscard]] const InputState& GetState() const noexcept override;

    // ── Mouse-specific shortcuts ──────────────────────────────────────────────

    [[nodiscard]] int32_t GetRelX()    const noexcept { return m_state.mouse.relX; }
    [[nodiscard]] int32_t GetRelY()    const noexcept { return m_state.mouse.relY; }
    [[nodiscard]] int32_t GetAbsX()    const noexcept { return m_state.mouse.absX; }
    [[nodiscard]] int32_t GetAbsY()    const noexcept { return m_state.mouse.absY; }
    [[nodiscard]] float   GetScrollX() const noexcept { return m_state.mouse.scrollX; }
    [[nodiscard]] float   GetScrollY() const noexcept { return m_state.mouse.scrollY; }

private:
    DeviceId   m_id;
    InputState m_state;
};

} // namespace SagaEngine::Input