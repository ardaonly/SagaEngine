/// @file SDLInputBackend.cpp
/// @brief SDL2/SDL3 implementation of IPlatformInputBackend.
///
/// This is the ONLY .cpp in the entire engine that includes SDL headers.
/// All SDL types are confined here. Nothing above this file sees SDL.

#include "SagaEngine/Input/Backends/SDL/SDLInputBackend.h"

// SDL version guard
#ifndef SAGA_SDL_VERSION
    #define SAGA_SDL_VERSION 2
#endif

#if SAGA_SDL_VERSION == 3
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_gamepad.h>
    #define SAGA_SDL_SCANCODE(e)   ((e).key.scancode)
    #define SAGA_SDL_KEYDOWN       SDL_EVENT_KEY_DOWN
    #define SAGA_SDL_KEYUP         SDL_EVENT_KEY_UP
    #define SAGA_SDL_MOUSEMOTION   SDL_EVENT_MOUSE_MOTION
    #define SAGA_SDL_MOUSEBUTTONDN SDL_EVENT_MOUSE_BUTTON_DOWN
    #define SAGA_SDL_MOUSEBUTTONUP SDL_EVENT_MOUSE_BUTTON_UP
    #define SAGA_SDL_MOUSEWHEEL    SDL_EVENT_MOUSE_WHEEL
    #define SAGA_SDL_TEXTINPUT     SDL_EVENT_TEXT_INPUT
    #define SAGA_SDL_QUIT          SDL_EVENT_QUIT
    #define SAGA_SDL_GAMEPADDOWN   SDL_EVENT_GAMEPAD_BUTTON_DOWN
    #define SAGA_SDL_GAMEPADUP     SDL_EVENT_GAMEPAD_BUTTON_UP
    #define SAGA_SDL_GAMEPADAXIS   SDL_EVENT_GAMEPAD_AXIS_MOTION
    #define SAGA_SDL_GAMEPADADD    SDL_EVENT_GAMEPAD_ADDED
    #define SAGA_SDL_GAMEPADREMOVE SDL_EVENT_GAMEPAD_REMOVED
    using SDLGamepadHandle = SDL_Gamepad*;
    #define SAGA_SDL_OPEN_GAMEPAD(id)  SDL_OpenGamepad(id)
    #define SAGA_SDL_CLOSE_GAMEPAD(h)  SDL_CloseGamepad((SDLGamepadHandle)(h))
    #define SAGA_SDL_GAMEPAD_ID(e)     ((e).gdevice.which)
#else
    // SDL2
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_gamecontroller.h>
    #define SAGA_SDL_SCANCODE(e)   ((e).key.keysym.scancode)
    #define SAGA_SDL_KEYDOWN       SDL_KEYDOWN
    #define SAGA_SDL_KEYUP         SDL_KEYUP
    #define SAGA_SDL_MOUSEMOTION   SDL_MOUSEMOTION
    #define SAGA_SDL_MOUSEBUTTONDN SDL_MOUSEBUTTONDOWN
    #define SAGA_SDL_MOUSEBUTTONUP SDL_MOUSEBUTTONUP
    #define SAGA_SDL_MOUSEWHEEL    SDL_MOUSEWHEEL
    #define SAGA_SDL_TEXTINPUT     SDL_TEXTINPUT
    #define SAGA_SDL_QUIT          SDL_QUIT
    #define SAGA_SDL_GAMEPADDOWN   SDL_CONTROLLERBUTTONDOWN
    #define SAGA_SDL_GAMEPADUP     SDL_CONTROLLERBUTTONUP
    #define SAGA_SDL_GAMEPADAXIS   SDL_CONTROLLERAXISMOTION
    #define SAGA_SDL_GAMEPADADD    SDL_CONTROLLERDEVICEADDED
    #define SAGA_SDL_GAMEPADREMOVE SDL_CONTROLLERDEVICEREMOVED
    using SDLGamepadHandle = SDL_GameController*;
    #define SAGA_SDL_OPEN_GAMEPAD(id)  SDL_GameControllerOpen(id)
    #define SAGA_SDL_CLOSE_GAMEPAD(h)  SDL_GameControllerClose((SDLGamepadHandle)(h))
    #define SAGA_SDL_GAMEPAD_ID(e)     ((e).cdevice.which)
#endif

#include <cstring>
#include <algorithm>

namespace SagaEngine::Platform
{

using Input::KeyCode;
using Input::MouseButton;
using Input::GamepadButton;
using Input::GamepadAxis;
using Input::ModifierFlags;

// Scancode mapping table
// Indexed by SDL_Scancode. Unknown entries map to KeyCode::Unknown.
// Table is built once at compile time.

static KeyCode BuildScancodeTable(int sdlCode) noexcept
{
    // clang-format off
    switch (sdlCode)
    {
    // Alphanumeric
    case SDL_SCANCODE_A: return KeyCode::A;  case SDL_SCANCODE_B: return KeyCode::B;
    case SDL_SCANCODE_C: return KeyCode::C;  case SDL_SCANCODE_D: return KeyCode::D;
    case SDL_SCANCODE_E: return KeyCode::E;  case SDL_SCANCODE_F: return KeyCode::F;
    case SDL_SCANCODE_G: return KeyCode::G;  case SDL_SCANCODE_H: return KeyCode::H;
    case SDL_SCANCODE_I: return KeyCode::I;  case SDL_SCANCODE_J: return KeyCode::J;
    case SDL_SCANCODE_K: return KeyCode::K;  case SDL_SCANCODE_L: return KeyCode::L;
    case SDL_SCANCODE_M: return KeyCode::M;  case SDL_SCANCODE_N: return KeyCode::N;
    case SDL_SCANCODE_O: return KeyCode::O;  case SDL_SCANCODE_P: return KeyCode::P;
    case SDL_SCANCODE_Q: return KeyCode::Q;  case SDL_SCANCODE_R: return KeyCode::R;
    case SDL_SCANCODE_S: return KeyCode::S;  case SDL_SCANCODE_T: return KeyCode::T;
    case SDL_SCANCODE_U: return KeyCode::U;  case SDL_SCANCODE_V: return KeyCode::V;
    case SDL_SCANCODE_W: return KeyCode::W;  case SDL_SCANCODE_X: return KeyCode::X;
    case SDL_SCANCODE_Y: return KeyCode::Y;  case SDL_SCANCODE_Z: return KeyCode::Z;

    case SDL_SCANCODE_0: return KeyCode::Num0; case SDL_SCANCODE_1: return KeyCode::Num1;
    case SDL_SCANCODE_2: return KeyCode::Num2; case SDL_SCANCODE_3: return KeyCode::Num3;
    case SDL_SCANCODE_4: return KeyCode::Num4; case SDL_SCANCODE_5: return KeyCode::Num5;
    case SDL_SCANCODE_6: return KeyCode::Num6; case SDL_SCANCODE_7: return KeyCode::Num7;
    case SDL_SCANCODE_8: return KeyCode::Num8; case SDL_SCANCODE_9: return KeyCode::Num9;

    // Modifiers
    case SDL_SCANCODE_LSHIFT:  return KeyCode::LShift;
    case SDL_SCANCODE_RSHIFT:  return KeyCode::RShift;
    case SDL_SCANCODE_LCTRL:   return KeyCode::LCtrl;
    case SDL_SCANCODE_RCTRL:   return KeyCode::RCtrl;
    case SDL_SCANCODE_LALT:    return KeyCode::LAlt;
    case SDL_SCANCODE_RALT:    return KeyCode::RAlt;
    case SDL_SCANCODE_LGUI:    return KeyCode::LMeta;
    case SDL_SCANCODE_RGUI:    return KeyCode::RMeta;

    // Control keys
    case SDL_SCANCODE_ESCAPE:    return KeyCode::Escape;
    case SDL_SCANCODE_RETURN:    return KeyCode::Enter;
    case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
    case SDL_SCANCODE_TAB:       return KeyCode::Tab;
    case SDL_SCANCODE_SPACE:     return KeyCode::Space;
    case SDL_SCANCODE_DELETE:    return KeyCode::Delete;
    case SDL_SCANCODE_INSERT:    return KeyCode::Insert;
    case SDL_SCANCODE_HOME:      return KeyCode::Home;
    case SDL_SCANCODE_END:       return KeyCode::End;
    case SDL_SCANCODE_PAGEUP:    return KeyCode::PageUp;
    case SDL_SCANCODE_PAGEDOWN:  return KeyCode::PageDown;
    case SDL_SCANCODE_CAPSLOCK:  return KeyCode::CapsLock;
    case SDL_SCANCODE_NUMLOCKCLEAR: return KeyCode::NumLock;
    case SDL_SCANCODE_SCROLLLOCK:   return KeyCode::ScrollLock;
    case SDL_SCANCODE_PRINTSCREEN:  return KeyCode::PrintScreen;
    case SDL_SCANCODE_PAUSE:        return KeyCode::Pause;

    // Arrows
    case SDL_SCANCODE_LEFT:  return KeyCode::Left;
    case SDL_SCANCODE_RIGHT: return KeyCode::Right;
    case SDL_SCANCODE_UP:    return KeyCode::Up;
    case SDL_SCANCODE_DOWN:  return KeyCode::Down;

    // Function keys
    case SDL_SCANCODE_F1:  return KeyCode::F1;  case SDL_SCANCODE_F2:  return KeyCode::F2;
    case SDL_SCANCODE_F3:  return KeyCode::F3;  case SDL_SCANCODE_F4:  return KeyCode::F4;
    case SDL_SCANCODE_F5:  return KeyCode::F5;  case SDL_SCANCODE_F6:  return KeyCode::F6;
    case SDL_SCANCODE_F7:  return KeyCode::F7;  case SDL_SCANCODE_F8:  return KeyCode::F8;
    case SDL_SCANCODE_F9:  return KeyCode::F9;  case SDL_SCANCODE_F10: return KeyCode::F10;
    case SDL_SCANCODE_F11: return KeyCode::F11; case SDL_SCANCODE_F12: return KeyCode::F12;

    // Numpad
    case SDL_SCANCODE_KP_0: return KeyCode::NumPad0;
    case SDL_SCANCODE_KP_1: return KeyCode::NumPad1;
    case SDL_SCANCODE_KP_2: return KeyCode::NumPad2;
    case SDL_SCANCODE_KP_3: return KeyCode::NumPad3;
    case SDL_SCANCODE_KP_4: return KeyCode::NumPad4;
    case SDL_SCANCODE_KP_5: return KeyCode::NumPad5;
    case SDL_SCANCODE_KP_6: return KeyCode::NumPad6;
    case SDL_SCANCODE_KP_7: return KeyCode::NumPad7;
    case SDL_SCANCODE_KP_8: return KeyCode::NumPad8;
    case SDL_SCANCODE_KP_9: return KeyCode::NumPad9;
    case SDL_SCANCODE_KP_ENTER:    return KeyCode::NumPadEnter;
    case SDL_SCANCODE_KP_PLUS:     return KeyCode::NumPadPlus;
    case SDL_SCANCODE_KP_MINUS:    return KeyCode::NumPadMinus;
    case SDL_SCANCODE_KP_MULTIPLY: return KeyCode::NumPadMultiply;
    case SDL_SCANCODE_KP_DIVIDE:   return KeyCode::NumPadDivide;
    case SDL_SCANCODE_KP_PERIOD:   return KeyCode::NumPadDecimal;

    // Punctuation
    case SDL_SCANCODE_COMMA:         return KeyCode::Comma;
    case SDL_SCANCODE_PERIOD:        return KeyCode::Period;
    case SDL_SCANCODE_SLASH:         return KeyCode::Slash;
    case SDL_SCANCODE_BACKSLASH:     return KeyCode::Backslash;
    case SDL_SCANCODE_SEMICOLON:     return KeyCode::Semicolon;
    case SDL_SCANCODE_APOSTROPHE:    return KeyCode::Apostrophe;
    case SDL_SCANCODE_LEFTBRACKET:   return KeyCode::LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET:  return KeyCode::RightBracket;
    case SDL_SCANCODE_GRAVE:         return KeyCode::Grave;
    case SDL_SCANCODE_MINUS:         return KeyCode::Minus;
    case SDL_SCANCODE_EQUALS:        return KeyCode::Equals;

    default: return KeyCode::Unknown;
    }
    // clang-format on
}

// Constructor / Destructor

SDLInputBackend::SDLInputBackend(SDL_Window* window)
    : m_window(window)
{}

SDLInputBackend::~SDLInputBackend()
{
    if (m_initialized) Shutdown();
}

// Lifecycle

bool SDLInputBackend::Initialize()
{
    if (m_initialized) return true;

#if SAGA_SDL_VERSION == 3
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
#else
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
#endif
    {
        // SDL already initialized by Application — not a fatal error
        // if the subystem init fails here, gamepads just won't work.
    }

    m_initialized    = true;
    m_quitRequested  = false;
    return true;
}

void SDLInputBackend::Shutdown()
{
    if (!m_initialized) return;

    // Close all open gamepad handles
    for (auto& slot : m_gamepads)
    {
        if (slot.handle)
        {
            SAGA_SDL_CLOSE_GAMEPAD(slot.handle);
            slot = {};
        }
    }

    m_initialized = false;
}

// PollRawEvents

void SDLInputBackend::PollRawEvents(Input::RawInputFrame& outFrame)
{
    outFrame.Clear();
    outFrame.frameTimestampUs =
        static_cast<uint64_t>(SDL_GetTicks()) * 1000ull; // ms → µs

    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        const uint64_t ts = outFrame.frameTimestampUs;

        switch (ev.type)
        {
        // Keyboard
        case SAGA_SDL_KEYDOWN:
        case SAGA_SDL_KEYUP:
        {
            const KeyCode code = BuildScancodeTable(
                static_cast<int>(SAGA_SDL_SCANCODE(ev))
            );
            if (code == KeyCode::Unknown) break;

            // Build modifier flags from current SDL modifier state
            const SDL_Keymod km = SDL_GetModState();
            ModifierFlags mods = ModifierFlags::None;
            if (km & KMOD_SHIFT) mods = mods | ModifierFlags::Shift;
            if (km & KMOD_CTRL)  mods = mods | ModifierFlags::Ctrl;
            if (km & KMOD_ALT)   mods = mods | ModifierFlags::Alt;
            if (km & KMOD_GUI)   mods = mods | ModifierFlags::Meta;

            outFrame.keys.push_back({
                .code       = code,
                .pressed    = (ev.type == SAGA_SDL_KEYDOWN),
                .repeat     = (ev.key.repeat != 0),
                .modifiers  = mods,
                .timestampUs = ts
            });
            break;
        }

        // Mouse motion
        case SAGA_SDL_MOUSEMOTION:
            outFrame.mouseMoves.push_back({
                .absX = ev.motion.x,
                .absY = ev.motion.y,
                .relX = ev.motion.xrel,
                .relY = ev.motion.yrel,
                .timestampUs = ts
            });
            break;

        // Mouse buttons
        case SAGA_SDL_MOUSEBUTTONDN:
        case SAGA_SDL_MOUSEBUTTONUP:
        {
            MouseButton btn = MouseButton::Unknown;
            switch (ev.button.button)
            {
            case SDL_BUTTON_LEFT:   btn = MouseButton::Left;   break;
            case SDL_BUTTON_RIGHT:  btn = MouseButton::Right;  break;
            case SDL_BUTTON_MIDDLE: btn = MouseButton::Middle; break;
            case SDL_BUTTON_X1:     btn = MouseButton::X1;     break;
            case SDL_BUTTON_X2:     btn = MouseButton::X2;     break;
            default: break;
            }
            if (btn == MouseButton::Unknown) break;

            outFrame.mouseButtons.push_back({
                .button     = btn,
                .pressed    = (ev.type == SAGA_SDL_MOUSEBUTTONDN),
                .x          = ev.button.x,
                .y          = ev.button.y,
                .clickCount = ev.button.clicks,
                .timestampUs = ts
            });
            break;
        }

        // Mouse scroll
        case SAGA_SDL_MOUSEWHEEL:
            outFrame.mouseScrolls.push_back({
#if SAGA_SDL_VERSION == 3
                .deltaX    = ev.wheel.x,
                .deltaY    = ev.wheel.y,
                .isPrecise = true,
#else
                .deltaX    = static_cast<float>(ev.wheel.x),
                .deltaY    = static_cast<float>(ev.wheel.y),
                .isPrecise = false,
#endif
                .timestampUs = ts
            });
            break;

        // Text input
        case SAGA_SDL_TEXTINPUT:
            if (m_textEnabled)
            {
                Input::RawTextEvent te{};
                std::strncpy(te.utf8, ev.text.text, 4);
                te.timestampUs = ts;
                outFrame.text.push_back(te);
            }
            break;

        // Gamepad buttons
        case SAGA_SDL_GAMEPADDOWN:
        case SAGA_SDL_GAMEPADUP:
        {
#if SAGA_SDL_VERSION == 3
            const auto button = MapSDLGamepadButton(ev.gbutton.button);
            const uint32_t devId = FindDeviceId(ev.gbutton.which);
#else
            const auto button = MapSDLGamepadButton(ev.cbutton.button);
            const uint32_t devId = FindDeviceId(ev.cbutton.which);
#endif
            if (button == GamepadButton::Unknown) break;

            outFrame.gamepadButtons.push_back({
                .deviceId    = devId,
                .button      = button,
                .pressed     = (ev.type == SAGA_SDL_GAMEPADDOWN),
                .timestampUs = ts
            });
            break;
        }

        // Gamepad axes
        case SAGA_SDL_GAMEPADAXIS:
        {
#if SAGA_SDL_VERSION == 3
            const auto axis  = MapSDLGamepadAxis(ev.gaxis.axis);
            const float value = NormalizeSDLAxis(ev.gaxis.value);
            const uint32_t devId = FindDeviceId(ev.gaxis.which);
#else
            const auto axis  = MapSDLGamepadAxis(ev.caxis.axis);
            const float value = NormalizeSDLAxis(ev.caxis.value);
            const uint32_t devId = FindDeviceId(ev.caxis.which);
#endif
            if (axis == GamepadAxis::Unknown) break;

            outFrame.gamepadAxes.push_back({
                .deviceId    = devId,
                .axis        = axis,
                .value       = value,
                .timestampUs = ts
            });
            break;
        }

        // Gamepad connect
        case SAGA_SDL_GAMEPADADD:
        {
            const int sdlIndex = static_cast<int>(SAGA_SDL_GAMEPAD_ID(ev));

            // Find a free slot
            for (auto& slot : m_gamepads)
            {
                if (!slot.handle)
                {
                    SDLGamepadHandle handle =
                        reinterpret_cast<SDLGamepadHandle>(
                            SAGA_SDL_OPEN_GAMEPAD(sdlIndex)
                        );
                    if (!handle) break;

                    slot.handle   = handle;
                    slot.deviceId = m_nextGamepadDeviceId++;

                    outFrame.gamepadConnections.push_back({
                        .deviceId    = slot.deviceId,
                        .connected   = true,
                        .timestampUs = ts
                    });
                    break;
                }
            }
            break;
        }

        // Gamepad disconnect
        case SAGA_SDL_GAMEPADREMOVE:
        {
            for (auto& slot : m_gamepads)
            {
                if (slot.handle)
                {
                    // SDL2: instanceId in cdevice.which; SDL3: same field
                    outFrame.gamepadConnections.push_back({
                        .deviceId    = slot.deviceId,
                        .connected   = false,
                        .timestampUs = ts
                    });
                    SAGA_SDL_CLOSE_GAMEPAD(slot.handle);
                    slot = {};
                    break;
                }
            }
            break;
        }

        // Quit
        case SAGA_SDL_QUIT:
            m_quitRequested = true;
            break;

        default:
            break;
        }
    }
}

// Text / Cursor

void SDLInputBackend::SetTextInputEnabled(bool enabled)
{
    m_textEnabled = enabled;
    if (enabled) SDL_StartTextInput();
    else         SDL_StopTextInput();
}

void SDLInputBackend::WarpCursor(int32_t x, int32_t y)
{
    if (m_window)
        SDL_WarpMouseInWindow(m_window, x, y);
}

void SDLInputBackend::SetCursorVisible(bool visible)
{
#if SAGA_SDL_VERSION == 3
    visible ? SDL_ShowCursor() : SDL_HideCursor();
#else
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
#endif
}

void SDLInputBackend::SetCursorLocked(bool locked)
{
    m_cursorLocked = locked;
    SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
}

// Static mapping helpers

Input::KeyCode SDLInputBackend::MapSDLScancode(int scancode) noexcept
{
    return BuildScancodeTable(scancode);
}

GamepadButton SDLInputBackend::MapSDLGamepadButton(uint8_t btn) noexcept
{
    // clang-format off
#if SAGA_SDL_VERSION == 3
    switch (static_cast<SDL_GamepadButton>(btn))
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:           return GamepadButton::South;
    case SDL_GAMEPAD_BUTTON_EAST:            return GamepadButton::East;
    case SDL_GAMEPAD_BUTTON_WEST:            return GamepadButton::West;
    case SDL_GAMEPAD_BUTTON_NORTH:           return GamepadButton::North;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:   return GamepadButton::LeftBumper;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:  return GamepadButton::RightBumper;
    case SDL_GAMEPAD_BUTTON_START:           return GamepadButton::Start;
    case SDL_GAMEPAD_BUTTON_BACK:            return GamepadButton::Select;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:      return GamepadButton::LeftThumb;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:     return GamepadButton::RightThumb;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:         return GamepadButton::DPadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:       return GamepadButton::DPadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:       return GamepadButton::DPadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:      return GamepadButton::DPadRight;
    default:                                 return GamepadButton::Unknown;
    }
#else
    switch (static_cast<SDL_GameControllerButton>(btn))
    {
    case SDL_CONTROLLER_BUTTON_A:             return GamepadButton::South;
    case SDL_CONTROLLER_BUTTON_B:             return GamepadButton::East;
    case SDL_CONTROLLER_BUTTON_X:             return GamepadButton::West;
    case SDL_CONTROLLER_BUTTON_Y:             return GamepadButton::North;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return GamepadButton::LeftBumper;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return GamepadButton::RightBumper;
    case SDL_CONTROLLER_BUTTON_START:         return GamepadButton::Start;
    case SDL_CONTROLLER_BUTTON_BACK:          return GamepadButton::Select;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return GamepadButton::LeftThumb;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return GamepadButton::RightThumb;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return GamepadButton::DPadUp;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return GamepadButton::DPadDown;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return GamepadButton::DPadLeft;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return GamepadButton::DPadRight;
    default:                                  return GamepadButton::Unknown;
    }
#endif
    // clang-format on
}

GamepadAxis SDLInputBackend::MapSDLGamepadAxis(uint8_t axis) noexcept
{
    // clang-format off
#if SAGA_SDL_VERSION == 3
    switch (static_cast<SDL_GamepadAxis>(axis))
    {
    case SDL_GAMEPAD_AXIS_LEFTX:              return GamepadAxis::LeftX;
    case SDL_GAMEPAD_AXIS_LEFTY:              return GamepadAxis::LeftY;
    case SDL_GAMEPAD_AXIS_RIGHTX:             return GamepadAxis::RightX;
    case SDL_GAMEPAD_AXIS_RIGHTY:             return GamepadAxis::RightY;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:       return GamepadAxis::LeftTrigger;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:      return GamepadAxis::RightTrigger;
    default:                                  return GamepadAxis::Unknown;
    }
#else
    switch (static_cast<SDL_GameControllerAxis>(axis))
    {
    case SDL_CONTROLLER_AXIS_LEFTX:           return GamepadAxis::LeftX;
    case SDL_CONTROLLER_AXIS_LEFTY:           return GamepadAxis::LeftY;
    case SDL_CONTROLLER_AXIS_RIGHTX:          return GamepadAxis::RightX;
    case SDL_CONTROLLER_AXIS_RIGHTY:          return GamepadAxis::RightY;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:     return GamepadAxis::LeftTrigger;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:    return GamepadAxis::RightTrigger;
    default:                                  return GamepadAxis::Unknown;
    }
#endif
    // clang-format on
}

float SDLInputBackend::NormalizeSDLAxis(int16_t value) noexcept
{
    // SDL axis range: [-32768, 32767]
    // We normalize to [-1.0, 1.0], clamping the asymmetric range.
    if (value < 0)
        return static_cast<float>(value) / 32768.f;
    else
        return static_cast<float>(value) / 32767.f;
}

uint32_t SDLInputBackend::FindDeviceId(int sdlJoystickId) const noexcept
{
    for (const auto& slot : m_gamepads)
    {
        if (slot.handle)
        {
#if SAGA_SDL_VERSION == 3
            const SDL_JoystickID id =
                SDL_GetGamepadID(reinterpret_cast<SDL_Gamepad*>(slot.handle));
#else
            const SDL_JoystickID id = SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(
                    reinterpret_cast<SDL_GameController*>(slot.handle)
                )
            );
#endif
            if (id == sdlJoystickId) return slot.deviceId;
        }
    }
    return Input::kInvalidDeviceId;
}

} // namespace SagaEngine::Platform