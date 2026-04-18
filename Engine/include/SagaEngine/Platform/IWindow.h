/// @file IWindow.h
/// @brief Platform-agnostic window interface for the SagaEngine windowing layer.

#pragma once

#include <cstdint>
#include <string>

namespace Saga {

// ─── Window Configuration ─────────────────────────────────────────────────────

/// Initialization parameters passed to IWindow::Init().
/// All fields carry engine-wide defaults; override per launch target.
struct WindowDesc
{
    std::string title       = "SagaEngine";  ///< Title bar text.
    uint32_t    width       = 1280;          ///< Client area width in pixels.
    uint32_t    height      = 720;           ///< Client area height in pixels.
    bool        vsync       = true;          ///< Synchronize present to monitor refresh.
    bool        resizable   = true;          ///< Allow runtime resize by the OS.
    bool        highDPI     = false;         ///< Enable high-DPI / Retina support.
    bool        borderless  = false;         ///< Create borderless window (no decorations).
    int         displayIndex = 0;            ///< Initial display index (0 = primary).
};

// ─── Window Interface ─────────────────────────────────────────────────────────

/// Abstracts a single OS window and its associated event pump.
///
/// Lifecycle contract:
///   Init() → [game loop: PollEvents / Present] → Shutdown()
///
/// Ownership: the window owns its native handle.
/// The RHI swap chain does NOT own the window — it borrows GetNativeHandle().
class IWindow
{
public:
    virtual ~IWindow() = default;

    /// Initialize the native window using the given descriptor.
    /// Returns false on failure; engine must not call any other method in that case.
    virtual bool Init(const WindowDesc& desc) = 0;

    /// Drain the OS event queue, updating ShouldClose() and input state.
    /// Must be called once per frame before any game logic.
    virtual void PollEvents() = 0;

    /// Flush the back buffer to screen.
    /// Once RHI is connected this delegates to the swap chain; until then it is a no-op.
    virtual void Present() = 0;

    /// Release all native resources.
    /// Safe to call even if Init() failed.
    virtual void Shutdown() = 0;

    // ─── State Queries ────────────────────────────────────────────────────────

    /// Returns true once a quit event (OS close button or ESC) has been received.
    [[nodiscard]] virtual bool ShouldClose() const noexcept = 0;

    /// Returns the toolkit-level handle (e.g. SDL_Window* for SDL).
    /// Used internally by platform code. Prefer GetOSNativeHandle() for
    /// passing to graphics backends.
    [[nodiscard]] virtual void*    GetNativeHandle() const noexcept = 0;

    /// Returns the OS-level window handle required by graphics APIs:
    /// HWND on Windows, X11 Window (as void*) on Linux/X11, wl_surface*
    /// on Wayland, NSWindow* on macOS. Returns nullptr if unavailable.
    /// This keeps SDL/platform details out of application code.
    [[nodiscard]] virtual void*    GetOSNativeHandle() const noexcept { return nullptr; }

    /// Set the window title bar text at runtime.
    virtual void SetTitle(const std::string& title) { (void)title; }

    /// Mark that an RHI swap chain owns presentation for this window.
    /// When true, Present() becomes a no-op — the RHI handles flipping.
    void SetRHIOwnsPresent(bool owns) noexcept { m_RHIOwnsPresent = owns; }

    /// Query whether an RHI swap chain is managing presentation.
    [[nodiscard]] bool IsRHIOwnsPresent() const noexcept { return m_RHIOwnsPresent; }

    /// Current client area width in pixels (may differ from WindowDesc after resize).
    [[nodiscard]] virtual uint32_t GetWidth()  const noexcept = 0;

    /// Current client area height in pixels (may differ from WindowDesc after resize).
    [[nodiscard]] virtual uint32_t GetHeight() const noexcept = 0;

protected:
    bool m_RHIOwnsPresent = false; ///< True when an RHI manages presentation.
};

} // namespace Saga