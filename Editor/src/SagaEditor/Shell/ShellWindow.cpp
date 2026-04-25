/// @file ShellWindow.cpp
/// @brief OS window abstraction for the editor — wraps the platform window layer.

#include "SagaEditor/Shell/ShellWindow.h"

// SDL2 is the current platform backend. The include is intentionally scoped to
// this translation unit so no SDL type leaks into editor headers.
#include <SDL2/SDL.h>

namespace SagaEditor
{

// ─── Construction ─────────────────────────────────────────────────────────────

ShellWindow::ShellWindow()  = default;
ShellWindow::~ShellWindow() { Shutdown(); }

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ShellWindow::Init(const ShellWindowConfig& cfg)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return false;

    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (cfg.maximized)
        flags |= SDL_WINDOW_MAXIMIZED;

    m_handle = SDL_CreateWindow(
        cfg.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg.width, cfg.height,
        flags);

    if (!m_handle)
        return false;

    m_width     = cfg.width;
    m_height    = cfg.height;
    m_maximized = cfg.maximized;
    return true;
}

bool ShellWindow::PollEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
            return false;

        if (ev.type == SDL_WINDOWEVENT)
        {
            if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                m_width  = ev.window.data1;
                m_height = ev.window.data2;
                if (m_onResize)
                    m_onResize(m_width, m_height);
            }
            else if (ev.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                if (m_onClose)
                    m_onClose();
                return false;
            }
        }
    }
    return true;
}

void ShellWindow::Present()
{
    SDL_UpdateWindowSurface(static_cast<SDL_Window*>(m_handle));
}

void ShellWindow::Shutdown()
{
    if (m_handle)
    {
        SDL_DestroyWindow(static_cast<SDL_Window*>(m_handle));
        m_handle = nullptr;
    }
    SDL_Quit();
}

// ─── Runtime Control ──────────────────────────────────────────────────────────

void ShellWindow::SetTitle(const std::string& title)
{
    SDL_SetWindowTitle(static_cast<SDL_Window*>(m_handle), title.c_str());
}

void ShellWindow::SetSize(int width, int height)
{
    SDL_SetWindowSize(static_cast<SDL_Window*>(m_handle), width, height);
    m_width  = width;
    m_height = height;
}

void ShellWindow::SetMaximized(bool maximized)
{
    auto* win = static_cast<SDL_Window*>(m_handle);
    if (maximized)
        SDL_MaximizeWindow(win);
    else
        SDL_RestoreWindow(win);

    m_maximized = maximized;
}

int  ShellWindow::GetWidth()       const noexcept { return m_width;     }
int  ShellWindow::GetHeight()      const noexcept { return m_height;    }
bool ShellWindow::IsMaximized()    const noexcept { return m_maximized; }
void* ShellWindow::GetNativeHandle() const noexcept { return m_handle;  }

// ─── Callbacks ────────────────────────────────────────────────────────────────

void ShellWindow::SetOnResize(ResizeCallback cb) { m_onResize = std::move(cb); }
void ShellWindow::SetOnClose(CloseCallback   cb) { m_onClose  = std::move(cb); }

} // namespace SagaEditor
