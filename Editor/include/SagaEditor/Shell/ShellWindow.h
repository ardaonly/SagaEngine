/// @file ShellWindow.h
/// @brief Thin facade over `IUIMainWindow` — keeps the editor UI-toolkit-agnostic.
///
/// Architectural rule: the editor never includes SDL. The editor calls into
/// the `IUIMainWindow` / `IUIFactory` / `IUIApplication` abstraction; a
/// concrete UI backend (Qt today) lives only inside `Editor/UI/Qt/`.
/// `ShellWindow` exists so legacy callers that took an `OS window` pointer
/// keep compiling without learning about the abstraction; new code should
/// reach for `IUIMainWindow` directly.

#pragma once

#include <functional>
#include <memory>
#include <string>

namespace SagaEditor
{

class IUIFactory;
class IUIMainWindow;

// ─── Window Configuration ─────────────────────────────────────────────────────

/// Startup parameters for the editor OS window. Same shape as before so
/// existing call sites continue to compile.
struct ShellWindowConfig
{
    std::string title     = "SagaEditor"; ///< Window title bar text.
    int         width     = 1600;         ///< Initial client-area width in pixels.
    int         height    = 900;          ///< Initial client-area height in pixels.
    bool        maximized = false;        ///< Open maximized on first launch.
};

// ─── Window ───────────────────────────────────────────────────────────────────

/// Owns an `IUIMainWindow` produced by an `IUIFactory`. Every operation
/// forwards through the abstraction; no UI-toolkit type ever appears here
/// or in any public header. The class deliberately keeps the original
/// public surface (Init / PollEvents / Present / Shutdown / setters /
/// getters / callbacks) so legacy callers compile unchanged.
class ShellWindow
{
public:
    using ResizeCallback = std::function<void(int width, int height)>;
    using CloseCallback  = std::function<void()>;

    ShellWindow();
    ~ShellWindow();

    ShellWindow(const ShellWindow&)            = delete;
    ShellWindow& operator=(const ShellWindow&) = delete;
    ShellWindow(ShellWindow&&) noexcept;
    ShellWindow& operator=(ShellWindow&&) noexcept;

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    /// Build the underlying `IUIMainWindow` through `factory`. Returns
    /// false when the factory refuses or returns nullptr. Calling Init
    /// twice without an intervening Shutdown is a no-op that returns false.
    [[nodiscard]] bool Init(IUIFactory& factory, const ShellWindowConfig& cfg);

    /// Pump-style hook kept for API compatibility. Qt is event-driven —
    /// the application's `Run` owns the timing — so this returns true
    /// while the window is alive and false once it has been closed.
    [[nodiscard]] bool PollEvents();

    /// Present hook kept for API compatibility. Qt repaints automatically;
    /// the call is a no-op so any legacy "tick → poll → present" loop
    /// keeps running without effect.
    void Present();

    /// Tear down the window through the abstraction.
    void Shutdown();

    // ─── Runtime Control ──────────────────────────────────────────────────────

    void SetTitle(const std::string& title);
    void SetSize(int width, int height);
    void SetMaximized(bool maximized);

    [[nodiscard]] int  GetWidth()       const noexcept;
    [[nodiscard]] int  GetHeight()      const noexcept;
    [[nodiscard]] bool IsMaximized()    const noexcept;

    /// Native window handle returned by the active UI backend.
    /// On the Qt backend this is a `QWidget*`. Callers that need the
    /// handle must cast at the backend boundary; no other code should
    /// dereference this.
    [[nodiscard]] void* GetNativeHandle() const noexcept;

    // ─── Callbacks ────────────────────────────────────────────────────────────

    void SetOnResize(ResizeCallback cb);
    void SetOnClose(CloseCallback   cb);

    // ─── Access ───────────────────────────────────────────────────────────────

    /// Direct access to the underlying `IUIMainWindow`. New code should
    /// prefer this — `ShellWindow` is a transitional facade.
    [[nodiscard]] IUIMainWindow* MainWindow() noexcept;
    [[nodiscard]] const IUIMainWindow* MainWindow() const noexcept;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace SagaEditor
