/// @file ViewportInput.h
/// @brief Framework-free input descriptors fed into the camera controller.

#pragma once

#include <cstdint>

namespace SagaEditor
{

// ─── Mouse Buttons ────────────────────────────────────────────────────────────

/// Bitmask of mouse buttons currently held. Matches the order the
/// `WorldViewportPanel` Qt event filter uses when forwarding events,
/// but the enum is intentionally framework-free so the controller
/// stays independent of the UI backend.
enum class MouseButton : std::uint8_t
{
    None    = 0,
    Left    = 1 << 0, ///< Primary button — used for selection and gizmo drag.
    Right   = 1 << 1, ///< Secondary button — used for fly-cam look.
    Middle  = 1 << 2, ///< Wheel button — used for pan.
    Back    = 1 << 3, ///< Back side button (rare on viewport input).
    Forward = 1 << 4, ///< Forward side button (rare on viewport input).
};

/// Bitwise helpers — `MouseButton` is treated as a flag set everywhere
/// the camera controller sees input.
[[nodiscard]] constexpr MouseButton
operator|(MouseButton a, MouseButton b) noexcept
{
    return static_cast<MouseButton>(static_cast<std::uint8_t>(a) |
                                     static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr MouseButton
operator&(MouseButton a, MouseButton b) noexcept
{
    return static_cast<MouseButton>(static_cast<std::uint8_t>(a) &
                                     static_cast<std::uint8_t>(b));
}

constexpr MouseButton& operator|=(MouseButton& a, MouseButton b) noexcept
{
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool HasButton(MouseButton mask, MouseButton b) noexcept
{
    return (mask & b) == b;
}

// ─── Keyboard Modifiers ───────────────────────────────────────────────────────

/// Bitmask of modifier keys held when the input event was raised.
/// Tracks the modifiers the camera controller actually cares about —
/// platform-specific keys (Cmd on macOS, Super on Linux) are mapped
/// onto `Ctrl` at the event-source layer.
enum class KeyModifiers : std::uint8_t
{
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
};

[[nodiscard]] constexpr KeyModifiers
operator|(KeyModifiers a, KeyModifiers b) noexcept
{
    return static_cast<KeyModifiers>(static_cast<std::uint8_t>(a) |
                                      static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr KeyModifiers
operator&(KeyModifiers a, KeyModifiers b) noexcept
{
    return static_cast<KeyModifiers>(static_cast<std::uint8_t>(a) &
                                      static_cast<std::uint8_t>(b));
}

constexpr KeyModifiers& operator|=(KeyModifiers& a, KeyModifiers b) noexcept
{
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool
HasModifier(KeyModifiers mask, KeyModifiers m) noexcept
{
    return (mask & m) == m;
}

// ─── Fly-Camera Translation Mask ──────────────────────────────────────────────

/// Bitmask of fly-camera translation directions currently engaged
/// through the keyboard. Mapped from WASDQE / arrow keys at the event
/// source.
enum class FlyDirection : std::uint8_t
{
    None     = 0,
    Forward  = 1 << 0, ///< W / Up arrow.
    Backward = 1 << 1, ///< S / Down arrow.
    Left     = 1 << 2, ///< A / Left arrow.
    Right    = 1 << 3, ///< D / Right arrow.
    Up       = 1 << 4, ///< E / PageUp.
    Down     = 1 << 5, ///< Q / PageDown.
};

[[nodiscard]] constexpr FlyDirection
operator|(FlyDirection a, FlyDirection b) noexcept
{
    return static_cast<FlyDirection>(static_cast<std::uint8_t>(a) |
                                      static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr FlyDirection
operator&(FlyDirection a, FlyDirection b) noexcept
{
    return static_cast<FlyDirection>(static_cast<std::uint8_t>(a) &
                                      static_cast<std::uint8_t>(b));
}

constexpr FlyDirection& operator|=(FlyDirection& a, FlyDirection b) noexcept
{
    a = a | b;
    return a;
}

[[nodiscard]] constexpr bool
HasDirection(FlyDirection mask, FlyDirection d) noexcept
{
    return (mask & d) == d;
}

// ─── Per-Frame Input Snapshot ─────────────────────────────────────────────────

/// Snapshot of viewport input state for a single frame. The camera
/// controller consumes one of these per `Tick` call; the event source
/// is responsible for accumulating mouse delta and scroll between
/// ticks. Pixel coordinates use the panel's local space (origin at
/// the top-left).
struct ViewportInputState
{
    MouseButton  buttons       = MouseButton::None;
    KeyModifiers modifiers     = KeyModifiers::None;
    FlyDirection flyMask       = FlyDirection::None;

    /// Cursor delta in panel pixels since the previous frame.
    /// Positive `dx` is right; positive `dy` is down (panel-local).
    float        mouseDeltaX   = 0.0f;
    float        mouseDeltaY   = 0.0f;

    /// Vertical scroll wheel ticks accumulated this frame. Positive
    /// values mean "wheel rolled forward" (zoom in for the orbit
    /// controller).
    float        scrollDelta   = 0.0f;

    /// Cursor position in panel pixels at the end of the frame. Used
    /// by the gizmo overlay to hit-test handles.
    float        cursorX       = 0.0f;
    float        cursorY       = 0.0f;

    /// Panel size in pixels. The controller divides delta by panel
    /// size when computing orbit speed so the look-feel does not
    /// change with viewport resolution.
    float        viewportW     = 0.0f;
    float        viewportH     = 0.0f;

    /// Frame time in seconds. Fly-camera translation scales by this
    /// so movement speed is wall-clock-time stable.
    float        deltaSeconds  = 0.0f;
};

} // namespace SagaEditor
