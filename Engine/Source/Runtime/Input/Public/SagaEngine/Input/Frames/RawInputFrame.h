#pragma once

/// @file RawInputFrame.h
/// @brief Batch container for all raw platform events in one poll cycle.
///
/// Layer  : Input / Foundation
/// Purpose: IPlatformInputBackend fills one RawInputFrame per PollRawEvents()
///          call. The frame is passed down to InputDevices which consume
///          relevant events. Nothing above InputDevice ever sees this struct.
///
/// Design notes:
///   - Events are batched, never delivered one at a time. This enables
///     consistent per-frame snapshots and avoids interleaving issues.
///   - Timestamps are in microseconds from the platform clock.
///   - Backend owns no state: it just collects and reports.

#include "SagaEngine/Input/Types/InputTypes.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace SagaEngine::Input
{

// Raw Event Structs
/// These are opaque to everything above the InputDevice layer.

struct RawKeyEvent
{
    KeyCode       code;
    bool          pressed;      ///< true = key down, false = key up
    bool          repeat;       ///< true = OS auto-repeat (skip for game input, keep for UI)
    ModifierFlags modifiers;    ///< Active modifiers at event time
    uint64_t      timestampUs;  ///< Platform timestamp in microseconds
};

struct RawMouseMoveEvent
{
    int32_t  absX, absY;        ///< Absolute cursor position within window
    int32_t  relX, relY;        ///< Delta since last event (raw input preferred)
    uint64_t timestampUs;
};

struct RawMouseButtonEvent
{
    MouseButton button;
    bool        pressed;
    int32_t     x, y;           ///< Cursor position at click time
    uint8_t     clickCount;     ///< 1 = single, 2 = double
    uint64_t    timestampUs;
};

struct RawMouseScrollEvent
{
    float    deltaX, deltaY;    ///< positive deltaY = scroll up
    bool     isPrecise;         ///< true if trackpad / high-res scroll
    uint64_t timestampUs;
};

struct RawGamepadButtonEvent
{
    DeviceId      deviceId;
    GamepadButton button;
    bool          pressed;
    uint64_t      timestampUs;
};

struct RawGamepadAxisEvent
{
    DeviceId    deviceId;
    GamepadAxis axis;
    float       value;          ///< Normalized: [-1.0, 1.0] sticks; [0.0, 1.0] triggers
    uint64_t    timestampUs;
};

struct RawGamepadConnectionEvent
{
    DeviceId    deviceId;
    bool        connected;      ///< true = connected, false = disconnected
    uint64_t    timestampUs;
};

/// Single UTF-8 character from OS text input (IME-composed, layout-aware).
struct RawTextEvent
{
    char     utf8[5]{};         ///< null-terminated, max 4 bytes
    uint64_t timestampUs;
};

// Frame Container

struct RawInputFrame
{
    uint32_t frameIndex      = 0;
    uint64_t frameTimestampUs = 0;  ///< Frame start timestamp

    std::vector<RawKeyEvent>              keys;
    std::vector<RawMouseMoveEvent>        mouseMoves;
    std::vector<RawMouseButtonEvent>      mouseButtons;
    std::vector<RawMouseScrollEvent>      mouseScrolls;
    std::vector<RawGamepadButtonEvent>    gamepadButtons;
    std::vector<RawGamepadAxisEvent>      gamepadAxes;
    std::vector<RawGamepadConnectionEvent>gamepadConnections;
    std::vector<RawTextEvent>             text;

    void Clear() noexcept
    {
        keys.clear();
        mouseMoves.clear();
        mouseButtons.clear();
        mouseScrolls.clear();
        gamepadButtons.clear();
        gamepadAxes.clear();
        gamepadConnections.clear();
        text.clear();
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return keys.empty()
            && mouseMoves.empty()
            && mouseButtons.empty()
            && mouseScrolls.empty()
            && gamepadButtons.empty()
            && gamepadAxes.empty()
            && gamepadConnections.empty()
            && text.empty();
    }
};

} // namespace SagaEngine::Input