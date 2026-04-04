#pragma once

/// @file InputTypes.h
/// @brief Platform-agnostic input vocabulary: KeyCode, MouseButton, GamepadButton, etc.
///
/// Layer  : Input / Foundation (no dependencies)
/// Purpose: Single source of truth for all input identifiers.
///          Platform backends map their native codes to these.
///          Upper layers (Mapping, Gameplay, Network) only use these types.
///
/// Rules  :
///   - Zero dependencies on Platform, SDL, Win32 or any backend.
///   - Zero dependencies on Gameplay, Network, or Engine subsystems.
///   - All enum values are explicit so they survive serialization across versions.

#include <cstdint>
#include <type_traits>
#include <string_view>

namespace SagaEngine::Input
{

// Device Identity

enum class DeviceType : uint8_t
{
    Unknown  = 0,
    Keyboard = 1,
    Mouse    = 2,
    Gamepad  = 3,
    Touch    = 4,
};

/// Opaque handle. 0 = invalid.
/// Backends assign stable IDs per physical device.
using DeviceId = uint32_t;
inline constexpr DeviceId kInvalidDeviceId = 0;

// ─── Key Codes ────────────────────────────────────────────────────────────────
/// All values are explicit to guarantee ABI stability across serialization.
/// Backends must map their native scancode/keycode to this enum.
/// DO NOT change existing values — add new ones at the end (before COUNT).

enum class KeyCode : uint16_t
{
    Unknown = 0,

    // Alphanumeric
    A = 1,  B = 2,  C = 3,  D = 4,  E = 5,  F = 6,  G = 7,
    H = 8,  I = 9,  J = 10, K = 11, L = 12, M = 13, N = 14,
    O = 15, P = 16, Q = 17, R = 18, S = 19, T = 20, U = 21,
    V = 22, W = 23, X = 24, Y = 25, Z = 26,

    Num0 = 30, Num1 = 31, Num2 = 32, Num3 = 33, Num4 = 34,
    Num5 = 35, Num6 = 36, Num7 = 37, Num8 = 38, Num9 = 39,

    // Modifiers
    LShift = 50, RShift = 51,
    LCtrl  = 52, RCtrl  = 53,
    LAlt   = 54, RAlt   = 55,
    LMeta  = 56, RMeta  = 57,   ///< Win / Cmd key

    // Control
    Escape    = 60,
    Enter     = 61,
    Backspace = 62,
    Tab       = 63,
    Space     = 64,
    Delete    = 65,
    Insert    = 66,
    Home      = 67,
    End       = 68,
    PageUp    = 69,
    PageDown  = 70,
    CapsLock  = 71,
    NumLock   = 72,
    ScrollLock= 73,
    PrintScreen= 74,
    Pause     = 75,

    // Arrows
    Left = 80, Right = 81, Up = 82, Down = 83,

    // Function
    F1 = 90,  F2 = 91,  F3 = 92,  F4 = 93,
    F5 = 94,  F6 = 95,  F7 = 96,  F8 = 97,
    F9 = 98,  F10 = 99, F11 = 100,F12 = 101,

    // Numpad
    NumPad0    = 110, NumPad1 = 111, NumPad2 = 112,
    NumPad3    = 113, NumPad4 = 114, NumPad5 = 115,
    NumPad6    = 116, NumPad7 = 117, NumPad8 = 118,
    NumPad9    = 119,
    NumPadEnter    = 120,
    NumPadPlus     = 121,
    NumPadMinus    = 122,
    NumPadMultiply = 123,
    NumPadDivide   = 124,
    NumPadDecimal  = 125,

    // Punctuation
    Comma        = 130,
    Period       = 131,
    Slash        = 132,
    Backslash    = 133,
    Semicolon    = 134,
    Apostrophe   = 135,
    LeftBracket  = 136,
    RightBracket = 137,
    Grave        = 138,  ///< ` ~
    Minus        = 139,
    Equals       = 140,

    COUNT = 256  ///< Sentinel — never use as a real key. Must remain last.
};

// Mouse

enum class MouseButton : uint8_t
{
    Unknown = 0,
    Left    = 1,
    Right   = 2,
    Middle  = 3,
    X1      = 4,
    X2      = 5,
    COUNT   = 6
};

// Gamepad

enum class GamepadButton : uint8_t
{
    Unknown      = 0,
    South        = 1,  ///< A (Xbox)  / Cross    (PS)
    East         = 2,  ///< B (Xbox)  / Circle   (PS)
    West         = 3,  ///< X (Xbox)  / Square   (PS)
    North        = 4,  ///< Y (Xbox)  / Triangle (PS)
    LeftBumper   = 5,
    RightBumper  = 6,
    LeftTriggerBtn  = 7,
    RightTriggerBtn = 8,
    Start        = 9,
    Select       = 10,
    LeftThumb    = 11,
    RightThumb   = 12,
    DPadUp       = 13,
    DPadDown     = 14,
    DPadLeft     = 15,
    DPadRight    = 16,
    COUNT        = 17
};

enum class GamepadAxis : uint8_t
{
    Unknown      = 0,
    LeftX        = 1,
    LeftY        = 2,
    RightX       = 3,
    RightY       = 4,
    LeftTrigger  = 5,  ///< [0.0, 1.0]
    RightTrigger = 6,  ///< [0.0, 1.0]
    COUNT        = 7
};

// Modifier Flags

enum class ModifierFlags : uint8_t
{
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
    Meta  = 1 << 3,
};

[[nodiscard]] constexpr ModifierFlags operator|(ModifierFlags a, ModifierFlags b) noexcept
{
    return static_cast<ModifierFlags>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
    );
}

[[nodiscard]] constexpr ModifierFlags operator&(ModifierFlags a, ModifierFlags b) noexcept
{
    return static_cast<ModifierFlags>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b)
    );
}

[[nodiscard]] constexpr bool HasModifier(ModifierFlags flags, ModifierFlags query) noexcept
{
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(query)) != 0;
}

// Utility

[[nodiscard]] std::string_view KeyCodeToString(KeyCode code) noexcept;
[[nodiscard]] std::string_view MouseButtonToString(MouseButton btn) noexcept;
[[nodiscard]] std::string_view GamepadButtonToString(GamepadButton btn) noexcept;

} // namespace SagaEngine::Input