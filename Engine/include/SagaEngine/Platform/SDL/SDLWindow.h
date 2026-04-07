/// @file SDLWindow.h
/// @brief SDL2 implementation of IWindow; manages a single SDL_Window and its event pump.

#pragma once

#include "SagaEngine/Platform/IWindow.h"
#include <SDL2/SDL.h>
#include <cstdint>

namespace Saga {

// ─── SDL Window Implementation ────────────────────────────────────────────────

/// Concrete IWindow backed by an SDL2 SDL_Window.
///
/// Responsibility boundary:
///   - Owns the SDL_Window* handle and its lifetime.
///   - Owns SDL library initialization (SDL_Init / SDL_Quit).
///   - Does NOT own the RHI swap chain; the RHI borrows GetNativeHandle()
///     to create its own surface/swap chain on top of this window.
///
/// Threading: all methods must be called from the main thread.
class SDLWindow final : public IWindow
{
public:
    SDLWindow()  = default;
    ~SDLWindow() override;

    // ─── IWindow Interface ────────────────────────────────────────────────────

    /// Initialize SDL, create the OS window, and set up platform-specific hints.
    /// Returns false if SDL_Init or SDL_CreateWindow fails; logs the error detail.
    bool Init(const WindowDesc& desc) override;

    /// Process all pending SDL events; sets ShouldClose() on SDL_QUIT or ESC.
    void PollEvents() override;

    /// Present the rendered frame.
    /// No-op until the RHI swap chain is connected via SetSwapChainCallback.
    void Present() override;

    /// Destroy the SDL_Window and call SDL_Quit.
    void Shutdown() override;

    // ─── State Queries ────────────────────────────────────────────────────────

    [[nodiscard]] bool     ShouldClose()      const noexcept override { return m_ShouldClose; }
    [[nodiscard]] void*    GetNativeHandle()  const noexcept override { return m_Window;      }
    [[nodiscard]] uint32_t GetWidth()         const noexcept override { return m_Width;       }
    [[nodiscard]] uint32_t GetHeight()        const noexcept override { return m_Height;      }

private:
    // ─── Internal Helpers ─────────────────────────────────────────────────────

    /// Handle a single SDL_Event; returns true if the event caused a state change.
    bool DispatchEvent(const SDL_Event& event) noexcept;

    // ─── Member Data ──────────────────────────────────────────────────────────

    SDL_Window* m_Window      = nullptr; ///< Owned SDL window handle.
    uint32_t    m_Width       = 0;       ///< Current client width in pixels.
    uint32_t    m_Height      = 0;       ///< Current client height in pixels.
    bool        m_ShouldClose = false;   ///< Set true when quit has been requested.
    bool        m_SDLOwned    = false;   ///< True if this instance called SDL_Init and must call SDL_Quit.
};

} // namespace Saga