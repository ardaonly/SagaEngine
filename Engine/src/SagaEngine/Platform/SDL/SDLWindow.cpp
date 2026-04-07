/// @file SDLWindow.cpp
/// @brief SDL2 implementation of IWindow; manages window lifecycle and event dispatch.

#include "SagaEngine/Platform/SDL/SDLWindow.h"
#include "SagaEngine/Core/Log/Log.h"
#include <SDL2/SDL.h>

namespace Saga {

SDLWindow::~SDLWindow()
{
    if (m_Window)
        Shutdown();
}

bool SDLWindow::Init(const WindowDesc& desc)
{
    m_ShouldClose = false; // önemli: her init'te temiz başla

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        LOG_ERROR("SDLWindow", "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    m_SDLOwned = true;

    const uint32_t flags =
        SDL_WINDOW_SHOWN |
        (desc.resizable ? SDL_WINDOW_RESIZABLE : 0u) |
        SDL_WINDOW_VULKAN;

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

void SDLWindow::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        LOG_INFO("SDLWindow", "Event received: type=%u", event.type);
        DispatchEvent(event);
    }
}

void SDLWindow::Present()
{
    // No-op until the RHI swap chain is registered.
}

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
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

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