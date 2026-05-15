/// @file SDLDebugRenderer2D.h
/// @brief SDL2 implementation of IDebugRenderer2D.

#pragma once

#include "SagaEngine/Platform/IDebugRenderer2D.h"

struct SDL_Renderer;

namespace Saga {

// ─── SDLDebugRenderer2D ──────────────────────────────────────────────────────

/// SDL_Renderer-backed 2D debug drawing.
class SDLDebugRenderer2D final : public IDebugRenderer2D
{
public:
    SDLDebugRenderer2D()  = default;
    ~SDLDebugRenderer2D() override;

    bool Init(IWindow& window, bool vsync = false) override;
    void Shutdown() override;

    void SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;
    void Clear()   override;
    void FillRect(int x, int y, int w, int h) override;
    void Present() override;

private:
    SDL_Renderer* m_renderer = nullptr;
};

} // namespace Saga
