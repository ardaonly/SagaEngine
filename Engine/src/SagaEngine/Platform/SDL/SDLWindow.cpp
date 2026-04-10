/// @file SDLWindow.cpp
/// @brief SDL2 implementation of IWindow; manages window lifecycle and event dispatch.

#include "SagaEngine/Platform/SDL/SDLWindow.h"
#include "SagaEngine/Core/Log/Log.h"
#include <SDL2/SDL.h>

namespace Saga {

// ─── Construction & Destruction ───────────────────────────────────────────────

SDLWindow::~SDLWindow()
{
    if (m_Window)
        Shutdown();
}

// ─── Initialization ───────────────────────────────────────────────────────────

bool SDLWindow::Init(const WindowDesc& desc)
{
    m_ShouldClose = false;
    m_VSync = desc.vsync;
    m_Title = desc.title;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        LOG_ERROR("SDLWindow", "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    m_SDLOwned = true;

    const uint32_t flags =
        SDL_WINDOW_SHOWN |
        (desc.resizable ? SDL_WINDOW_RESIZABLE : 0u);

    m_Window = SDL_CreateWindow(
        desc.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        static_cast<int>(desc.width),
        static_cast<int>(desc.height),
        flags
    );

    if (!m_Window)
    {
        LOG_ERROR("SDLWindow", "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        m_SDLOwned = false;
        return false;
    }

    m_Width  = desc.width;
    m_Height = desc.height;

    LOG_INFO("SDLWindow", "Window created: title=%s, size=%u x %u",
        desc.title.c_str(), m_Width, m_Height);

    return true;
}

// ─── Event Processing ─────────────────────────────────────────────────────────

void SDLWindow::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        DispatchEvent(event);
    }
}

// ─── Present ──────────────────────────────────────────────────────────────────

void SDLWindow::Present()
{
    // When RHI is not connected, just update the window surface.
    // This keeps the window responsive and visible.
    if (m_Window)
    {
        SDL_UpdateWindowSurface(m_Window);
    }
}

// ─── Extended Features ────────────────────────────────────────────────────────

void SDLWindow::SetTitle(const std::string& title)
{
    if (m_Window)
    {
        SDL_SetWindowTitle(m_Window, title.c_str());
        m_Title = title;
    }
}

void SDLWindow::SetSize(uint32_t width, uint32_t height)
{
    if (m_Window)
    {
        SDL_SetWindowSize(m_Window, static_cast<int>(width), static_cast<int>(height));
        m_Width = width;
        m_Height = height;
    }
}

void SDLWindow::SetFullscreen(bool fullscreen)
{
    if (m_Window && m_Fullscreen != fullscreen)
    {
        m_Fullscreen = fullscreen;
        SDL_SetWindowFullscreen(m_Window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        LOG_INFO("SDLWindow", "Fullscreen %s", fullscreen ? "enabled" : "disabled");
    }
}

void SDLWindow::SetVSync(bool vsync)
{
    m_VSync = vsync;
    // VSync is typically handled by the RHI swap chain; store for future use.
}

void SDLWindow::SetMinimumSize(uint32_t width, uint32_t height)
{
    if (m_Window)
    {
        SDL_SetWindowMinimumSize(m_Window, static_cast<int>(width), static_cast<int>(height));
    }
}

// ─── Event Dispatch ───────────────────────────────────────────────────────────

bool SDLWindow::DispatchEvent(const SDL_Event& event) noexcept
{
    switch (event.type)
    {
        case SDL_QUIT:
            LOG_INFO("SDLWindow", "SDL_QUIT received.");
            m_ShouldClose = true;
            return true;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                LOG_INFO("SDLWindow", "SDL_WINDOWEVENT_CLOSE received.");
                m_ShouldClose = true;
                return true;
            }

            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                m_Width  = static_cast<uint32_t>(event.window.data1);
                m_Height = static_cast<uint32_t>(event.window.data2);
                LOG_INFO("SDLWindow", "Resized to %u x %u", m_Width, m_Height);
                
                if (m_OnResize)
                    m_OnResize(m_Width, m_Height);
                
                return true;
            }

            if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
            {
                // Window was exposed - update surface to prevent black screen
                SDL_UpdateWindowSurface(m_Window);
                return true;
            }
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
            {
                LOG_INFO("SDLWindow", "ESC key pressed.");
                m_ShouldClose = true;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void SDLWindow::Shutdown()
{
    if (m_Window)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
        LOG_INFO("SDLWindow", "Window destroyed.");
    }

    if (m_SDLOwned)
    {
        SDL_Quit();
        m_SDLOwned = false;
        LOG_INFO("SDLWindow", "SDL subsystems released.");
    }

    m_ShouldClose = false;
}

} // namespace Saga