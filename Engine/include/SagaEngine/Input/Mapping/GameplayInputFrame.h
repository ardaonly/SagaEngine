#pragma once

/// @file GameplayInputFrame.h
/// @brief The final output of the input pipeline, consumed by gameplay only.
///
/// Layer  : Input / Mapping → Gameplay boundary
/// Purpose: Aggregates all resolved action events and analog values for
///          one simulation tick. This is the ONLY input data gameplay code
///          is allowed to read.
///
/// Gameplay code must NEVER read:
///   - KeyCode, MouseButton, GamepadButton
///   - InputState, RawInputFrame
///   - SDL events or any platform type
///
/// It reads ONLY:
///   - GameplayInputFrame::actions  → resolved action events
///   - GameplayInputFrame::move     → movement axes
///   - GameplayInputFrame::look     → look/camera axes
///
/// This struct is also the source for building an InputCommand when
/// the client needs to send input to the server.

#include "SagaEngine/Input/Mapping/InputAction.h"
#include <vector>
#include <cstdint>

namespace SagaEngine::Input
{

struct AnalogAxes2D
{
    float x = 0.f;
    float y = 0.f;
};

struct GameplayInputFrame
{
    uint32_t tick            = 0;     ///< Simulation tick this frame is for
    uint64_t frameTimestampUs = 0;    ///< Wall clock time

    /// All resolved action events this frame.
    std::vector<InputActionEvent> actions;

    /// Aggregated analog movement (WASD or left stick), normalized [-1, 1].
    AnalogAxes2D move;

    /// Aggregated look/camera delta (mouse or right stick).
    AnalogAxes2D look;

    /// Scroll wheel or zoom axis.
    float scroll = 0.f;

    // Query helpers

    [[nodiscard]] bool WasPressed(InputActionId id) const noexcept
    {
        for (const auto& e : actions)
            if (e.id == id && e.phase == ActionPhase::Pressed) return true;
        return false;
    }

    [[nodiscard]] bool IsHeld(InputActionId id) const noexcept
    {
        for (const auto& e : actions)
            if (e.id == id && (e.phase == ActionPhase::Pressed
                            || e.phase == ActionPhase::Held)) return true;
        return false;
    }

    [[nodiscard]] bool WasReleased(InputActionId id) const noexcept
    {
        for (const auto& e : actions)
            if (e.id == id && e.phase == ActionPhase::Released) return true;
        return false;
    }

    [[nodiscard]] float GetValue(InputActionId id) const noexcept
    {
        for (const auto& e : actions)
            if (e.id == id) return e.value;
        return 0.f;
    }

    void Clear() noexcept
    {
        actions.clear();
        move   = {};
        look   = {};
        scroll = 0.f;
    }
};

} // namespace SagaEngine::Input