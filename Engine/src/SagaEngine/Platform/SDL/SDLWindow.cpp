/// @file SDLWindow.cpp
/// @brief SDL2 implementation of IWindow; manages window lifecycle and event dispatch.
///
/// Features:
///   - Multi-monitor support: display enumeration, window placement
///   - Window icon: PNG/BMP/ICO loading via SDL_image or raw SDL surface
///   - Borderless mode: SDL_WINDOW_BORDERLESS for kiosk/overlay use cases
///   - High-DPI: SDL_WINDOW_ALLOW_HIGHDPI with drawable size queries

#include "SagaEngine/Platform/SDL/SDLWindow.h"
#include "SagaEngine/Core/Log/Log.h"
#include <SDL2/SDL.h>

// SDL_image is optional; fall back to SDL's built-in BMP loader if unavailable.
#ifdef SAGA_USE_SDL_IMAGE
    #include <SDL2/SDL_image.h>
#endif

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
    m_HighDPI = desc.highDPI;
    m_Borderless = desc.borderless;

    // Set high-DPI hint before SDL_Init.
    if (m_HighDPI)
    {
#if SDL_VERSION_ATLEAST(2, 0, 1)
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        LOG_ERROR("SDLWindow", "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    m_SDLOwned = true;

    uint32_t flags = SDL_WINDOW_SHOWN | (desc.resizable ? SDL_WINDOW_RESIZABLE : 0u);

    if (m_HighDPI)
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    if (m_Borderless)
        flags |= SDL_WINDOW_BORDERLESS;

    // Position window on the requested display.
    int windowX = SDL_WINDOWPOS_CENTERED_DISPLAY(desc.displayIndex);
    int windowY = SDL_WINDOWPOS_CENTERED_DISPLAY(desc.displayIndex);

    m_Window = SDL_CreateWindow(
        desc.title.c_str(),
        windowX,
        windowY,
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

    // Log display info for high-DPI setups.
    if (m_HighDPI)
    {
        const int drawableW = GetDrawableWidth();
        const int drawableH = GetDrawableHeight();
        const float dpiScale = GetDPIScale();
        LOG_INFO("SDLWindow", "High-DPI window: client %ux%u, drawable %ux%d, scale %.2f",
                 m_Width, m_Height, drawableW, drawableH, dpiScale);
    }

    // Log display count.
    const int displayCount = GetDisplayCount();
    LOG_INFO("SDLWindow", "Window created: title=%s, size=%ux%u, displays=%d",
             desc.title.c_str(), m_Width, m_Height, displayCount);

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

// ─── Multi-Monitor Support ────────────────────────────────────────────────────

int SDLWindow::GetDisplayCount() const noexcept
{
    return SDL_GetNumVideoDisplays();
}

std::string SDLWindow::GetDisplayName(int displayIndex) const noexcept
{
    if (!m_Window || displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays())
        return {};

    const char* name = SDL_GetDisplayName(displayIndex);
    return name ? std::string(name) : std::string();
}

bool SDLWindow::GetDisplayBounds(int displayIndex, int* outX, int* outY,
                                  int* outWidth, int* outHeight) const noexcept
{
    if (!m_Window || displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays())
        return false;

    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(displayIndex, &bounds) != 0)
    {
        LOG_WARN("SDLWindow", "SDL_GetDisplayBounds failed for display %d: %s",
                 displayIndex, SDL_GetError());
        return false;
    }

    if (outX) *outX = bounds.x;
    if (outY) *outY = bounds.y;
    if (outWidth) *outWidth = bounds.w;
    if (outHeight) *outHeight = bounds.h;
    return true;
}

void SDLWindow::MoveToDisplay(int displayIndex) noexcept
{
    if (!m_Window || displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays())
    {
        LOG_WARN("SDLWindow", "Invalid display index %d (max %d)",
                 displayIndex, SDL_GetNumVideoDisplays() - 1);
        return;
    }

    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(displayIndex, &bounds) != 0)
    {
        LOG_WARN("SDLWindow", "Failed to get bounds for display %d", displayIndex);
        return;
    }

    // Center the window on the target display.
    const int x = bounds.x + (bounds.w - static_cast<int>(m_Width)) / 2;
    const int y = bounds.y + (bounds.h - static_cast<int>(m_Height)) / 2;

    SDL_SetWindowPosition(m_Window, x, y);

    LOG_INFO("SDLWindow", "Window moved to display %d at (%d, %d)", displayIndex, x, y);
}

int SDLWindow::GetCurrentDisplayIndex() const noexcept
{
    if (!m_Window)
        return -1;

    return SDL_GetWindowDisplayIndex(m_Window);
}

// ─── Window Icon ──────────────────────────────────────────────────────────────

bool SDLWindow::SetIconFromFile(const char* iconPath) noexcept
{
    if (!m_Window || !iconPath)
        return false;

#ifdef SAGA_USE_SDL_IMAGE
    // Use SDL_image for PNG/ICO support.
    SDL_Surface* iconSurface = IMG_Load(iconPath);
#else
    // Fallback: SDL's built-in BMP loader only.
    SDL_Surface* iconSurface = SDL_LoadBMP(iconPath);
#endif

    if (!iconSurface)
    {
        LOG_ERROR("SDLWindow", "Failed to load icon from '%s': %s",
                  iconPath, SDL_GetError());
        return false;
    }

    SetIconFromMemory(
        reinterpret_cast<const uint8_t*>(iconSurface->pixels),
        iconSurface->w,
        iconSurface->h
    );

    SDL_FreeSurface(iconSurface);
    return true;
}

void SDLWindow::SetIconFromMemory(const uint8_t* data, int width, int height) noexcept
{
    if (!m_Window || !data || width <= 0 || height <= 0)
        return;

    // Create a 32-bit RGBA surface for the icon.
    SDL_Surface* iconSurface = SDL_CreateRGBSurfaceFrom(
        const_cast<uint8_t*>(data),
        width,
        height,
        32,            // bits per pixel
        width * 4,     // pitch (bytes per row)
        0x000000FFu,   // R mask
        0x0000FF00u,   // G mask
        0x00FF0000u,   // B mask
        0xFF000000u    // A mask
    );

    if (!iconSurface)
    {
        LOG_ERROR("SDLWindow", "SDL_CreateRGBSurfaceFrom failed for icon: %s", SDL_GetError());
        return;
    }

    SDL_SetWindowIcon(m_Window, iconSurface);
    SDL_FreeSurface(iconSurface);

    LOG_DEBUG("SDLWindow", "Window icon set: %dx%d pixels", width, height);
}

// ─── Borderless Mode ──────────────────────────────────────────────────────────

void SDLWindow::SetBorderless(bool borderless) noexcept
{
    if (!m_Window || m_Borderless == borderless)
        return;

    m_Borderless = borderless;

    if (borderless)
        SDL_SetWindowBordered(m_Window, SDL_FALSE);
    else
        SDL_SetWindowBordered(m_Window, SDL_TRUE);

    LOG_INFO("SDLWindow", "Borderless %s", borderless ? "enabled" : "disabled");
}

// ─── High-DPI Support ─────────────────────────────────────────────────────────

uint32_t SDLWindow::GetDrawableWidth() const noexcept
{
    if (!m_Window)
        return m_Width;

    int w = 0;
    SDL_GL_GetDrawableSize(m_Window, &w, nullptr);
    return (w > 0) ? static_cast<uint32_t>(w) : m_Width;
}

uint32_t SDLWindow::GetDrawableHeight() const noexcept
{
    if (!m_Window)
        return m_Height;

    int h = 0;
    SDL_GL_GetDrawableSize(m_Window, nullptr, &h);
    return (h > 0) ? static_cast<uint32_t>(h) : m_Height;
}

float SDLWindow::GetDPIScale() const noexcept
{
    if (!m_Window || !m_HighDPI)
        return 1.0f;

    float hdpi = 1.0f, vdpi = 1.0f;
    // SDL_GetWindowDisplayIndex may fail if window is not yet shown.
    const int displayIndex = SDL_GetWindowDisplayIndex(m_Window);
    if (displayIndex < 0)
        return 1.0f;

    if (SDL_GetDisplayDPI(displayIndex, nullptr, &hdpi, &vdpi) != 0)
        return 1.0f;

    // DPI is in dots per inch; 96 DPI = 1.0x scale on Windows.
    // On macOS Retina, this returns 2.0x (192 DPI).
    return hdpi / 96.0f;
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