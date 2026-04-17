/// @file SDLWindow.h
/// @brief SDL2 implementation of IWindow; manages a single SDL_Window and its event pump.

#pragma once

#include "SagaEngine/Platform/IWindow.h"
#include <SDL2/SDL.h>
#include <cstdint>
#include <string>
#include <functional>

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
    /// When no RHI swap chain is connected, this updates the window and
    /// maintains the frame timing. Once RHI is connected, it delegates to RHI.
    void Present() override;

    /// Destroy the SDL_Window and call SDL_Quit.
    void Shutdown() override;

    // ─── State Queries ────────────────────────────────────────────────────────

    [[nodiscard]] bool     ShouldClose()      const noexcept override { return m_ShouldClose; }
    [[nodiscard]] void*    GetNativeHandle()  const noexcept override { return m_Window;      }
    [[nodiscard]] uint32_t GetWidth()         const noexcept override { return m_Width;       }
    [[nodiscard]] uint32_t GetHeight()        const noexcept override { return m_Height;      }

    // ─── Extended Features ────────────────────────────────────────────────────

    /// Set the window title at runtime.
    void SetTitle(const std::string& title);

    /// Set the window size at runtime.
    void SetSize(uint32_t width, uint32_t height);

    /// Set whether the window should be fullscreen.
    void SetFullscreen(bool fullscreen);

    /// Set whether VSync is enabled.
    void SetVSync(bool vsync);

    /// Set a callback for window resize events.
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    void SetOnResize(ResizeCallback callback) { m_OnResize = std::move(callback); }

    /// Set the window's minimum size (0,0 = no limit).
    void SetMinimumSize(uint32_t width, uint32_t height);

    // ─── Multi-Monitor Support ────────────────────────────────────────────────

    /// Get the number of connected displays.
    [[nodiscard]] int GetDisplayCount() const noexcept;

    /// Get the name of a display by index.
    /// @param displayIndex  0-based display index.
    /// @return Display name string, or empty string on error.
    [[nodiscard]] std::string GetDisplayName(int displayIndex = 0) const noexcept;

    /// Get the display bounds (in global coordinates) for a display.
    /// @param displayIndex  0-based display index.
    /// @param outX          Output: display origin X.
    /// @param outY          Output: display origin Y.
    /// @param outWidth      Output: display width.
    /// @param outHeight     Output: display height.
    /// @return true on success; false if display index is invalid.
    [[nodiscard]] bool GetDisplayBounds(int displayIndex, int* outX, int* outY,
                                         int* outWidth, int* outHeight) const noexcept;

    /// Move the window to a specific display (centered on that display).
    /// @param displayIndex  0-based display index.
    void MoveToDisplay(int displayIndex) noexcept;

    /// Get the current display index the window is on.
    [[nodiscard]] int GetCurrentDisplayIndex() const noexcept;

    // ─── Window Icon ──────────────────────────────────────────────────────────

    /// Set the window icon from a file path (PNG, BMP, or ICO).
    /// @param iconPath  Path to the icon file.
    /// @return true on success; false if file not found or invalid format.
    bool SetIconFromFile(const char* iconPath) noexcept;

    /// Set the window icon from raw RGBA data.
    /// @param data   Raw pixel data (RGBA, 4 bytes per pixel).
    /// @param width  Icon width in pixels.
    /// @param height Icon height in pixels.
    void SetIconFromMemory(const uint8_t* data, int width, int height) noexcept;

    // ─── Borderless Mode ─────────────────────────────────────────────────────

    /// Set whether the window is borderless (no title bar, no window decorations).
    /// @param borderless  true for borderless; false for normal windowed.
    void SetBorderless(bool borderless) noexcept;

    /// Query whether the window is currently borderless.
    [[nodiscard]] bool IsBorderless() const noexcept { return m_Borderless; }

    // ─── High-DPI Support ─────────────────────────────────────────────────────

    /// Get the window size in screen coordinates (may differ from pixel size on
    /// high-DPI displays like Retina).
    [[nodiscard]] uint32_t GetDrawableWidth() const noexcept;
    [[nodiscard]] uint32_t GetDrawableHeight() const noexcept;

    /// Get the DPI scale factor (drawable pixels / window points).
    /// On a standard display this is 1.0; on a Retina/2x display it is 2.0.
    [[nodiscard]] float GetDPIScale() const noexcept;

private:
    // ─── Internal Helpers ─────────────────────────────────────────────────────

    /// Handle a single SDL_Event; returns true if the event caused a state change.
    bool DispatchEvent(const SDL_Event& event) noexcept;

    // ─── Member Data ──────────────────────────────────────────────────────────

    SDL_Window*   m_Window      = nullptr; ///< Owned SDL window handle.
    uint32_t      m_Width       = 0;       ///< Current client width in pixels.
    uint32_t      m_Height      = 0;       ///< Current client height in pixels.
    bool          m_ShouldClose = false;   ///< Set true when quit has been requested.
    bool          m_SDLOwned    = false;   ///< True if this instance called SDL_Init.
    bool          m_VSync       = true;    ///< VSync enabled state.
    bool          m_Fullscreen  = false;   ///< Fullscreen state.
    bool          m_Borderless  = false;   ///< Borderless window state.
    bool          m_HighDPI     = false;   ///< High-DPI aware window.
    std::string   m_Title;                 ///< Current window title.
    ResizeCallback m_OnResize;             ///< Window resize callback.
};

} // namespace Saga