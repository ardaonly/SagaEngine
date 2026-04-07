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
    std::string title  = "SagaEngine";  ///< Title bar text.
    uint32_t    width  = 1280;          ///< Client area width in pixels.
    uint32_t    height = 720;           ///< Client area height in pixels.
    bool        vsync  = true;          ///< Synchronize present to monitor refresh.
    bool        resizable = true;       ///< Allow runtime resize by the OS.
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

    /// Returns the platform-native window handle.
    /// Type depends on backend:  SDL_Window* (SDL), HWND (Win32), etc.
    /// Caller must not assume ownership — the window retains ownership.
    [[nodiscard]] virtual void*    GetNativeHandle() const noexcept = 0;

    /// Current client area width in pixels (may differ from WindowDesc after resize).
    [[nodiscard]] virtual uint32_t GetWidth()  const noexcept = 0;

    /// Current client area height in pixels (may differ from WindowDesc after resize).
    [[nodiscard]] virtual uint32_t GetHeight() const noexcept = 0;
};

} // namespace Saga