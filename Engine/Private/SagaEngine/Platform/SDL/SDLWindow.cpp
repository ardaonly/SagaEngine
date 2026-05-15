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
#if defined(__linux__) && defined(SDL_VIDEO_DRIVER_X11)
#   if defined(__has_include)
#       if !__has_include(<X11/Xlib.h>)
#           undef SDL_VIDEO_DRIVER_X11
#       endif
#   endif
#endif
#include <SDL2/SDL_syswm.h>
#include <cstring>

// SDL_image is optional; fall back to SDL's built-in BMP loader if unavailable.
#ifdef SAGA_USE_SDL_IMAGE
    #include <SDL2/SDL_image.h>
#endif

namespace Saga {

namespace
{

#if defined(__linux__)
struct SDLX11WindowInfoFallback
{
    void*     display  = nullptr;
    uintptr_t windowId = 0;
};

SDLX11WindowInfoFallback ReadX11WindowInfo(const SDL_SysWMinfo& wmInfo) noexcept
{
#   if defined(SDL_VIDEO_DRIVER_X11)
    return {
        wmInfo.info.x11.display,
        static_cast<uintptr_t>(wmInfo.info.x11.window),
    };
#   else
    // Some distro/package builds expose the X11 runtime backend but do not put
    // Xlib headers on the compile include path. SDL still writes the X11 pair
    // into the union area: Display* followed by Window/XID.
    struct RawX11Info
    {
        void*     display;
        uintptr_t window;
    };
    const auto* raw = reinterpret_cast<const RawX11Info*>(&wmInfo.info);
    return {raw->display, raw->window};
#   endif
}
#endif

} // namespace

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

    Uint32 flags = SDL_WINDOW_SHOWN | (desc.resizable ? SDL_WINDOW_RESIZABLE : 0u);

    if (m_HighDPI)
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    if (m_Borderless)
        flags |= SDL_WINDOW_BORDERLESS;

#if defined(__linux__)
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    if (videoDriver && std::strcmp(videoDriver, "wayland") == 0)
    {
        flags |= SDL_WINDOW_VULKAN;
    }
    else if (videoDriver && std::strcmp(videoDriver, "x11") == 0)
    {
        flags |= SDL_WINDOW_OPENGL;
    }
#endif

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

#if defined(__linux__)
    if (!m_Window && (flags & SDL_WINDOW_VULKAN) != 0)
    {
        LOG_WARN("SDLWindow", "SDL_CreateWindow with Vulkan flag failed: %s", SDL_GetError());
        flags &= ~SDL_WINDOW_VULKAN;
        m_Window = SDL_CreateWindow(
            desc.title.c_str(),
            windowX,
            windowY,
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            flags
        );
    }
#endif

    if (!m_Window)
    {
        LOG_ERROR("SDLWindow", "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        m_SDLOwned = false;
        return false;
    }

    m_Width  = desc.width;
    m_Height = desc.height;

#if defined(__linux__)
    if ((flags & SDL_WINDOW_OPENGL) != 0)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        m_GLContext = SDL_GL_CreateContext(m_Window);
        if (!m_GLContext)
        {
            LOG_WARN("SDLWindow", "SDL_GL_CreateContext failed: %s", SDL_GetError());
        }
        else if (SDL_GL_MakeCurrent(m_Window, static_cast<SDL_GLContext>(m_GLContext)) != 0)
        {
            LOG_WARN("SDLWindow", "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
            SDL_GL_DeleteContext(static_cast<SDL_GLContext>(m_GLContext));
            m_GLContext = nullptr;
        }
        else
        {
            SDL_GL_SetSwapInterval(desc.vsync ? 1 : 0);
            LOG_INFO("SDLWindow", "OpenGL context created for X11 fallback.");
        }
    }
#endif

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
    LOG_INFO("SDLWindow", "Window created: title=%s, size=%ux%u, displays=%d, driver=%s",
             desc.title.c_str(), m_Width, m_Height, displayCount,
             SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");

    return true;
}

// ─── Event Processing ─────────────────────────────────────────────────────────

void SDLWindow::PollEvents()
{
    // Pump the OS event queue, then selectively extract only the events
    // that belong to the window layer.  Keyboard, mouse, and gamepad
    // events stay in the queue so the InputManager (SDLInputBackend) can
    // consume them later in the same frame via SDL_PollEvent.
    SDL_PumpEvents();

    SDL_Event events[32];

    // ── SDL_QUIT ─────────────────────────────────────────────────────
    int count = SDL_PeepEvents(events, 32, SDL_GETEVENT, SDL_QUIT, SDL_QUIT);
    for (int i = 0; i < count; ++i)
        DispatchEvent(events[i]);

    // ── SDL_WINDOWEVENT (resize, close, expose, …) ──────────────────
    count = SDL_PeepEvents(events, 32, SDL_GETEVENT, SDL_WINDOWEVENT, SDL_WINDOWEVENT);
    for (int i = 0; i < count; ++i)
        DispatchEvent(events[i]);
}

// ─── Present ──────────────────────────────────────────────────────────────────

void SDLWindow::Present()
{
    // When an RHI swap chain is connected (D3D12, Vulkan, etc.) it owns
    // the back buffer and calls Present() itself in EndFrame().
    // SDL_UpdateWindowSurface conflicts with the RHI's swap chain and
    // causes an immediate crash on D3D12, so skip it entirely.
    if (m_Window && !IsRHIOwnsPresent())
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
                // When an RHI swap chain owns presentation, the software surface
                // must not be touched — SDL_UpdateWindowSurface conflicts with
                // the D3D12/Vulkan swap chain and causes a crash.
                if (!IsRHIOwnsPresent())
                    SDL_UpdateWindowSurface(m_Window);
                return true;
            }
            break;

        // Keyboard events are NOT processed here — they belong to the
        // InputManager.  SDLWindow::PollEvents uses SDL_PeepEvents so
        // keyboard events stay in the queue for SDLInputBackend.

        default:
            break;
    }

    return false;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void SDLWindow::Shutdown()
{
    if (m_GLContext)
    {
        SDL_GL_MakeCurrent(nullptr, nullptr);
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(m_GLContext));
        m_GLContext = nullptr;
        LOG_INFO("SDLWindow", "OpenGL context destroyed.");
    }

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

// ─── OS Native Handle ────────────────────────────────────────────────────────

void* SDLWindow::GetOSNativeHandle() const noexcept
{
    if (!m_Window) return nullptr;

    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(m_Window, &wmInfo))
    {
        LOG_ERROR("SDLWindow", "SDL_GetWindowWMInfo failed: %s", SDL_GetError());
        return nullptr;
    }

    m_NativeHandle = {};

#if defined(_WIN32)
    m_NativeHandle.backend = NativeWindowBackend::Win32;
    m_NativeHandle.window  = wmInfo.info.win.window;
    return &m_NativeHandle;
#elif defined(__linux__)
    switch (wmInfo.subsystem)
    {
        case SDL_SYSWM_X11:
        {
            const auto x11 = ReadX11WindowInfo(wmInfo);
            m_NativeHandle.backend  = NativeWindowBackend::X11;
            m_NativeHandle.display  = x11.display;
            m_NativeHandle.windowId = x11.windowId;
            m_NativeHandle.window   = reinterpret_cast<void*>(m_NativeHandle.windowId);
            return &m_NativeHandle;
        }
#   if defined(SDL_VIDEO_DRIVER_WAYLAND)
        case SDL_SYSWM_WAYLAND:
            m_NativeHandle.backend = NativeWindowBackend::Wayland;
            m_NativeHandle.display = wmInfo.info.wl.display;
            m_NativeHandle.surface = wmInfo.info.wl.surface;
            m_NativeHandle.window  = wmInfo.info.wl.surface;
            return &m_NativeHandle;
#   endif
        default:
            LOG_ERROR("SDLWindow", "Unsupported SDL window subsystem: %d",
                      static_cast<int>(wmInfo.subsystem));
            return nullptr;
    }
#elif defined(__APPLE__)
    m_NativeHandle.backend = NativeWindowBackend::Cocoa;
    m_NativeHandle.window  = wmInfo.info.cocoa.window;
    return &m_NativeHandle;
#else
    return nullptr;
#endif
}

} // namespace Saga
