/// @file ShellWindow.cpp
/// @brief Thin facade over `IUIMainWindow` — no UI toolkit appears here.
///
/// Every operation delegates through the abstraction so the editor stays
/// UI-toolkit-agnostic: the Qt backend lives only in `Editor/UI/Qt/`, and
/// swapping Qt for a future backend is a `IUIFactory` swap, not a global
/// `#include` rewrite. SDL is intentionally absent from the editor — it
/// has no place in authoring tools.

#include "SagaEditor/Shell/ShellWindow.h"

#include "SagaEditor/UI/IUIFactory.h"
#include "SagaEditor/UI/IUIMainWindow.h"

#include <utility>

namespace SagaEditor
{

// ─── Internal State ───────────────────────────────────────────────────────────

struct ShellWindow::State
{
    std::unique_ptr<IUIMainWindow> window;       ///< Owned through the abstraction.
    int                            width     = 0; ///< Cached for stable getters.
    int                            height    = 0;
    bool                           maximized = false;
    ResizeCallback                 onResize;     ///< Forwarded by the backend.
    CloseCallback                  onClose;      ///< Forwarded by the backend.
};

// ─── Construction ─────────────────────────────────────────────────────────────

ShellWindow::ShellWindow()
    : m_state(std::make_unique<State>())
{}

ShellWindow::~ShellWindow()
{
    Shutdown();
}

ShellWindow::ShellWindow(ShellWindow&&) noexcept            = default;
ShellWindow& ShellWindow::operator=(ShellWindow&&) noexcept = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ShellWindow::Init(IUIFactory& factory, const ShellWindowConfig& cfg)
{
    if (m_state->window != nullptr)
    {
        // Already initialised; refuse a second Init so the cached
        // dimensions and callbacks stay honest.
        return false;
    }

    auto window = factory.CreateMainWindow(cfg.title, cfg.width, cfg.height);
    if (!window)
    {
        return false;
    }

    // Wire the close callback through the abstraction so the legacy
    // ShellWindow::SetOnClose surface keeps working.
    window->SetOnClose([this]() {
        if (m_state->onClose) m_state->onClose();
    });

    m_state->width     = cfg.width;
    m_state->height    = cfg.height;
    m_state->maximized = cfg.maximized;
    m_state->window    = std::move(window);

    if (cfg.maximized)
    {
        m_state->window->ShowMaximized();
    }
    else
    {
        m_state->window->Show();
    }
    return true;
}

bool ShellWindow::PollEvents()
{
    // Qt is event-driven: `IUIApplication::Run` owns the timing. Legacy
    // callers that still drive a tick/poll/present loop see a no-op
    // here that simply reports whether the window is still alive.
    return m_state->window != nullptr;
}

void ShellWindow::Present()
{
    // Repaints are scheduled by the UI toolkit; nothing to do here.
}

void ShellWindow::Shutdown()
{
    if (m_state && m_state->window)
    {
        m_state->window->Close();
        m_state->window.reset();
    }
}

// ─── Runtime Control ──────────────────────────────────────────────────────────

void ShellWindow::SetTitle(const std::string& title)
{
    if (m_state->window) m_state->window->SetTitle(title);
}

void ShellWindow::SetSize(int width, int height)
{
    if (m_state->window) m_state->window->SetSize(width, height);
    m_state->width  = width;
    m_state->height = height;
}

void ShellWindow::SetMaximized(bool maximized)
{
    if (!m_state->window) return;
    if (maximized)
    {
        m_state->window->ShowMaximized();
    }
    else
    {
        m_state->window->Show();
    }
    m_state->maximized = maximized;
}

int ShellWindow::GetWidth() const noexcept
{
    return m_state->window ? m_state->window->GetWidth() : m_state->width;
}

int ShellWindow::GetHeight() const noexcept
{
    return m_state->window ? m_state->window->GetHeight() : m_state->height;
}

bool ShellWindow::IsMaximized() const noexcept
{
    return m_state->maximized;
}

void* ShellWindow::GetNativeHandle() const noexcept
{
    return m_state->window ? m_state->window->GetNativeHandle() : nullptr;
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void ShellWindow::SetOnResize(ResizeCallback cb)
{
    m_state->onResize = std::move(cb);
    // The abstraction does not yet expose a resize hook — once it does,
    // wire it here exactly like the close callback. For now the cached
    // dimensions update through SetSize so callers can poll.
}

void ShellWindow::SetOnClose(CloseCallback cb)
{
    m_state->onClose = std::move(cb);
}

// ─── Access ───────────────────────────────────────────────────────────────────

IUIMainWindow* ShellWindow::MainWindow() noexcept
{
    return m_state->window.get();
}

const IUIMainWindow* ShellWindow::MainWindow() const noexcept
{
    return m_state->window.get();
}

} // namespace SagaEditor
