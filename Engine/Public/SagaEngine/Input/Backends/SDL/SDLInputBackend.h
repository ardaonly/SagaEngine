#pragma once

/// @file SDLInputBackend.h
/// @brief SDL2/SDL3 implementation of IPlatformInputBackend.
///
/// Layer  : Platform / Backends / SDL
/// Purpose: Translates SDL_Event into RawInputFrame.
///          This is the ONLY file in the entire input system that includes SDL.
///          Everything above this layer is SDL-free.
///
/// Scope boundary:
///   SDLInputBackend → ONLY included by Engine/src/Platform/Backends/SDL/
///   SDLInputBackend → NEVER included by Input/, Mapping/, Commands/, Server/
///
/// Key mapping:
///   SDL_Scancode → SagaEngine::Input::KeyCode
///   SDL_MouseButtonEvent → SagaEngine::Input::RawMouseButtonEvent
///   SDL_GamepadButton → SagaEngine::Input::GamepadButton
///   etc.
///
///   The mapping table lives in SDLInputBackend.cpp and is validated at
///   static_assert level where possible.
///
/// SDL version:
///   Supports SDL2 and SDL3 via compile-time flag SAGA_SDL_VERSION (2 or 3).
///   SDL3 renamed several symbols; the #if guards are contained here.

#include "SagaEngine/Input/Backends/IPlatformInputBackend.h"
#include <cstdint>
#include <memory>
#include <string_view>

// Forward-declare SDL types so this header doesn't expose SDL to includers.
// The .cpp file includes the actual SDL headers.
struct SDL_Window;

namespace SagaEngine::Platform
{

class SDLInputBackend final : public IPlatformInputBackend
{
public:
    /// @param window  The SDL_Window to associate with (for text input rect etc.)
    ///                May be nullptr if not yet created; set via SetWindow().
    explicit SDLInputBackend(SDL_Window* window = nullptr);
    ~SDLInputBackend() override;

    SDLInputBackend(const SDLInputBackend&)            = delete;
    SDLInputBackend& operator=(const SDLInputBackend&) = delete;

    // IPlatformInputBackend

    bool Initialize() override;
    void Shutdown()   override;

    void PollRawEvents(Input::RawInputFrame& outFrame) override;

    void SetTextInputEnabled(bool enabled) override;
    void WarpCursor(int32_t x, int32_t y) override;
    void SetCursorVisible(bool visible)   override;
    void SetCursorLocked(bool locked)     override;

    [[nodiscard]] std::string_view GetBackendName() const noexcept override
    {
        return "SDL";
    }

    // SDL-specific

    /// Update the associated SDL_Window (e.g. after window recreation).
    void SetWindow(SDL_Window* window) noexcept { m_window = window; }

    /// Optionally consume SDL_QUIT events. Returns true if quit was requested.
    [[nodiscard]] bool WasQuitRequested() const noexcept { return m_quitRequested; }

private:

    /// Map SDL_Scancode → KeyCode. Returns KeyCode::Unknown for unmapped codes.
    [[nodiscard]] static Input::KeyCode MapSDLScancode(int scancode) noexcept;

    /// Map SDL_GameControllerButton / SDL_GamepadButton → GamepadButton.
    [[nodiscard]] static Input::GamepadButton MapSDLGamepadButton(uint8_t btn) noexcept;

    /// Map SDL_GameControllerAxis / SDL_GamepadAxis → GamepadAxis.
    [[nodiscard]] static Input::GamepadAxis MapSDLGamepadAxis(uint8_t axis) noexcept;

    /// Normalize SDL axis value [-32768, 32767] → [-1.0, 1.0].
    [[nodiscard]] static float NormalizeSDLAxis(int16_t value) noexcept;

    /// Map SDL Joystick ID to our internal Device ID
    [[nodiscard]] uint32_t FindDeviceId(int sdlJoystickId) const noexcept;

    SDL_Window* m_window         = nullptr;
    bool        m_initialized    = false;
    bool        m_textEnabled    = false;
    bool        m_cursorLocked   = false;
    bool        m_quitRequested  = false;

    /// Per-gamepad handle storage (opaque void* to avoid SDL type in header).
    struct GamepadSlot
    {
        void*    handle   = nullptr;  ///< SDL_GameController* / SDL_Gamepad*
        uint32_t deviceId = Input::kInvalidDeviceId;
    };

    static constexpr int kMaxGamepads = 8;
    GamepadSlot m_gamepads[kMaxGamepads]{};

    uint32_t m_nextGamepadDeviceId = 2000;
};

} // namespace SagaEngine::Platform