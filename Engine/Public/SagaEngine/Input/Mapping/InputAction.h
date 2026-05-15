#pragma once

/// @file InputAction.h
/// @brief Action identifiers and chord definitions for input mapping.
///
/// Layer  : Input / Mapping
/// Purpose: Defines the "vocabulary" of gameplay actions and the
///          physical key combinations that trigger them.
///
/// An InputChord is a physical input (key + modifiers, mouse button, gamepad
/// button, or analog axis + threshold). An InputActionId names a gameplay
/// action (MoveForward, Jump, Fire, etc.). InputActionMap binds chords
/// to action IDs.
///
/// Design notes:
///   - InputActionId is strong-typed but project-defined via a config file
///     or enum. The mapping layer treats it as an opaque integer so it
///     doesn't need to know all possible actions at compile time.
///   - InputActionEvent carries both the action ID and its current phase
///     (Pressed, Released, Held, Analog). Gameplay reads only this event;
///     it never sees a KeyCode.
///   - Chord matching supports modifier flags so Shift+Space differs from Space.

#include "SagaEngine/Input/Types/InputTypes.h"
#include <cstdint>
#include <variant>

namespace SagaEngine::Input
{

// Action Identity

/// Opaque action identifier. Project defines its own enum and casts to this.
/// E.g.:  enum class GameAction : InputActionId { MoveForward = 1, Jump = 2 };
using InputActionId = uint32_t;
inline constexpr InputActionId kInvalidActionId = 0;

// Action Phase

enum class ActionPhase : uint8_t
{
    Pressed  = 0,   ///< Edge: first frame the binding is active
    Held     = 1,   ///< Continuous: binding remains active
    Released = 2,   ///< Edge: first frame the binding goes inactive
    Analog   = 3,   ///< Continuous with a float value (axis, trigger)
};

// Input Chords
/// A chord = one physical input + optional modifier requirement.

struct KeyChord
{
    KeyCode       key;
    ModifierFlags modifiers = ModifierFlags::None;

    [[nodiscard]] bool Matches(KeyCode k, ModifierFlags mods) const noexcept
    {
        return key == k && modifiers == mods;
    }
};

struct MouseButtonChord
{
    MouseButton   button;
    ModifierFlags modifiers = ModifierFlags::None;
};

struct GamepadButtonChord
{
    GamepadButton button;
};

struct GamepadAxisChord
{
    GamepadAxis axis;
    float       threshold   = 0.2f;     ///< Dead zone
    float       direction   = 1.f;      ///< +1 or -1 for axis direction binding
};

using InputChord = std::variant<
    KeyChord,
    MouseButtonChord,
    GamepadButtonChord,
    GamepadAxisChord
>;

// Action Event
/// Produced by InputActionMap::Resolve(). Consumed by gameplay only.
/// Gameplay NEVER sees KeyCode, MouseButton, or GamepadButton.

struct InputActionEvent
{
    InputActionId id      = kInvalidActionId;
    ActionPhase   phase   = ActionPhase::Pressed;
    float         value   = 0.f;   ///< [0,1] for digital, analog for axis
    uint32_t      tick    = 0;     ///< Simulation tick this event belongs to
};

// Binding Entry

struct InputBinding
{
    InputActionId id;
    InputChord    chord;
    bool          consumeInput = true;  ///< If true, chord won't trigger other actions
};

} // namespace SagaEngine::Input