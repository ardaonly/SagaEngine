/// @file SDLDebugRenderer2D.cpp
/// @brief SDL2 implementation of IDebugRenderer2D.

#include "SagaEngine/Platform/SDL/SDLDebugRenderer2D.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Core/Log/Log.h"

#include <SDL2/SDL.h>

namespace Saga {

// ─── Lifecycle ───────────────────────────────────────────────────────────────

SDLDebugRenderer2D::~SDLDebugRenderer2D()
{
    Shutdown();
}

bool SDLDebugRenderer2D::Init(IWindow& window, bool vsync)
{
    auto* sdlWindow = static_cast<SDL_Window*>(window.GetNativeHandle());
    if (!sdlWindow)
    {
        LOG_ERROR("DebugRenderer2D", "Null SDL_Window handle.");
        return false;
    }

    Uint32 flags = 0;
    if (vsync)
        flags |= SDL_RENDERER_PRESENTVSYNC;

    m_renderer = SDL_CreateRenderer(sdlWindow, -1, flags);
    if (!m_renderer)
    {
        LOG_WARN("DebugRenderer2D", "SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    LOG_INFO("DebugRenderer2D", "SDL debug renderer created.");
    return true;
}

void SDLDebugRenderer2D::Shutdown()
{
    if (m_renderer)
    {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

void SDLDebugRenderer2D::SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (m_renderer)
        SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
}

void SDLDebugRenderer2D::Clear()
{
    if (m_renderer)
        SDL_RenderClear(m_renderer);
}

void SDLDebugRenderer2D::FillRect(int x, int y, int w, int h)
{
    if (m_renderer)
    {
        SDL_Rect rect = { x, y, w, h };
        SDL_RenderFillRect(m_renderer, &rect);
    }
}

void SDLDebugRenderer2D::Present()
{
    if (m_renderer)
        SDL_RenderPresent(m_renderer);
}

} // namespace Saga
