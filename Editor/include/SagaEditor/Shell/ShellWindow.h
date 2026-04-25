/// @file ShellWindow.h
/// @brief OS window abstraction for the editor — wraps the platform window layer.

#pragma once

#include <functional>
#include <string>

namespace SagaEditor
{

// ─── Window Configuration ─────────────────────────────────────────────────────

/// Startup parameters for the editor OS window.
struct ShellWindowConfig
{
    std::string title     = "SagaEditor"; ///< Window title bar text.
    int         width     = 1600;         ///< Initial client-area width in pixels.
    int         height    = 900;          ///< Initial client-area height in pixels.
    bool        maximized = false;        ///< Open maximized on first launch.
};

// ─── Window ───────────────────────────────────────────────────────────────────

/// Thin abstraction over the platform window (SDL, Win32, etc.).
/// Owns the OS handle, drives event polling, and exposes callbacks the shell
/// subscribes to without depending on any platform-specific type.
class ShellWindow
{
public:
    using ResizeCallback = std::function<void(int width, int height)>; ///< Fired on resize.
    using CloseCallback  = std::function<void()>;                       ///< Fired on close request.

    ShellWindow();
    ~ShellWindow();

    /// Create the OS window with the given parameters. Returns false on failure.
    [[nodiscard]] bool Init(const ShellWindowConfig& cfg);

    /// Pump OS events. Returns false when the window has been closed.
    [[nodiscard]] bool PollEvents();

    /// Present the rendered frame to the screen.
    void Present();

    /// Tear down the OS window and release platform resources.
    void Shutdown();

    // ─── Runtime Control ──────────────────────────────────────────────────────

    void SetTitle(const std::string& title);
    void SetSize(int width, int height);
    void SetMaximized(bool maximized);

    [[nodiscard]] int  GetWidth()  const noexcept;
    [[nodiscard]] int  GetHeight() const noexcept;
    [[nodiscard]] bool IsMaximized() const noexcept;

    /// Native window handle (SDL_Window* on SDL builds; cast at call site).
    [[nodiscard]] void* GetNativeHandle() const noexcept;

    // ─── Callbacks ────────────────────────────────────────────────────────────

    void SetOnResize(ResizeCallback cb);
    void SetOnClose(CloseCallback   cb);

private:
    void*          m_handle    = nullptr; ///< Opaque platform window pointer.
    int            m_width     = 0;       ///< Current client-area width.
    int            m_height    = 0;       ///< Current client-area height.
    bool           m_maximized = false;   ///< True when the window is maximized.
    ResizeCallback m_onResize;            ///< Subscriber for resize events.
    CloseCallback  m_onClose;             ///< Subscriber for close-request events.
};

} // namespace SagaEditor
