#include "DiligentGpuTestFixture.h"

#include <SDL.h>
#if defined(__linux__) && defined(SDL_VIDEO_DRIVER_X11)
#   if defined(__has_include)
#       if !__has_include(<X11/Xlib.h>)
#           undef SDL_VIDEO_DRIVER_X11
#       endif
#   endif
#endif
#include <SDL2/SDL_syswm.h>

#if defined(None)
#   undef None
#endif
#if defined(Bool)
#   undef Bool
#endif
#if defined(True)
#   undef True
#endif
#if defined(False)
#   undef False
#endif

#include <utility>

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

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

/// Extracts the platform native window handle from an SDL_Window.
/// Returns nullptr if the platform is not supported.
void* GetNativeHandle(SDL_Window* window)
{
    if (!window) return nullptr;
    static Saga::NativeWindowHandle handle{};

    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo))
        return nullptr;

    handle = {};

#if defined(_WIN32)
    handle.backend = Saga::NativeWindowBackend::Win32;
    handle.window  = wmInfo.info.win.window;
    return &handle;
#elif defined(__linux__)
    switch (wmInfo.subsystem)
    {
        case SDL_SYSWM_X11:
        {
            const auto x11 = ReadX11WindowInfo(wmInfo);
            handle.backend  = Saga::NativeWindowBackend::X11;
            handle.display  = x11.display;
            handle.windowId = x11.windowId;
            handle.window   = reinterpret_cast<void*>(handle.windowId);
            return &handle;
        }
#   if defined(SDL_VIDEO_DRIVER_WAYLAND)
        case SDL_SYSWM_WAYLAND:
            handle.backend = Saga::NativeWindowBackend::Wayland;
            handle.display = wmInfo.info.wl.display;
            handle.surface = wmInfo.info.wl.surface;
            handle.window  = wmInfo.info.wl.surface;
            return &handle;
#   endif
        default:
            return nullptr;
    }
#elif defined(__APPLE__)
    handle.backend = Saga::NativeWindowBackend::Cocoa;
    handle.window  = wmInfo.info.cocoa.window;
    return &handle;
#else
    return nullptr;
#endif
}


TestDiligentRenderBackend::TestDiligentRenderBackend()
    : m_backend(MakeConfig())
{
}

bool TestDiligentRenderBackend::Initialize(const SwapchainDesc& desc)
{
    return m_backend.Initialize(desc);
}

void TestDiligentRenderBackend::Shutdown()
{
    m_backend.Shutdown();
}

void TestDiligentRenderBackend::OnResize(std::uint32_t width, std::uint32_t height)
{
    m_backend.OnResize(width, height);
}

SagaEngine::Render::World::MeshId TestDiligentRenderBackend::CreateMesh(
    const SagaEngine::Render::MeshAsset& asset)
{
    return m_backend.CreateMesh(asset);
}

SagaEngine::Render::World::MaterialId TestDiligentRenderBackend::CreateMaterial(
    const SagaEngine::Render::MaterialRuntime& runtime)
{
    return m_backend.CreateMaterial(runtime);
}

void TestDiligentRenderBackend::DestroyMesh(SagaEngine::Render::World::MeshId id)
{
    m_backend.DestroyMesh(id);
}

void TestDiligentRenderBackend::DestroyMaterial(SagaEngine::Render::World::MaterialId id)
{
    m_backend.DestroyMaterial(id);
}

SagaEngine::Render::TextureHandle TestDiligentRenderBackend::CreateTexture(
    std::uint32_t width,
    std::uint32_t height,
    const std::uint8_t* rgba)
{
    return m_backend.CreateTexture(width, height, rgba);
}

void TestDiligentRenderBackend::DestroyTexture(SagaEngine::Render::TextureHandle texture)
{
    m_backend.DestroyTexture(texture);
}

void TestDiligentRenderBackend::BeginFrame()
{
    m_backend.BeginFrame();
}

void TestDiligentRenderBackend::Submit(const Camera& camera, const RenderView& view)
{
    m_backend.Submit(camera, view);
}

void TestDiligentRenderBackend::EndFrame()
{
    m_backend.EndFrame();
}

GraphicsBackendAPI TestDiligentRenderBackend::SelectedAPI() const noexcept
{
    return m_backend.SelectedAPI();
}

std::uint64_t TestDiligentRenderBackend::FrameIndex() const noexcept
{
    return m_backend.FrameIndex();
}

bool TestDiligentRenderBackend::IsInitialized() const noexcept
{
    return m_backend.IsInitialized();
}

RenderFrameDiagnostics TestDiligentRenderBackend::LastFrameDiagnostics() const noexcept
{
    return m_backend.LastFrameDiagnostics();
}

SagaEngine::Graphics::Backends::Diligent::Runtime::DiligentGraphicsRuntime&
TestDiligentRenderBackend::RuntimeForIntegrationTesting() noexcept
{
    return m_backend.RuntimeForIntegrationTesting();
}

RenderCaptureResult TestDiligentRenderBackend::CaptureCurrentColorFrame(
    RenderFrameCapture& capture)
{
    return m_backend.CaptureCurrentColorFrame(capture);
}

IRenderBackend& TestDiligentRenderBackend::BackendInterface() noexcept
{
    return m_backend;
}

RenderBackendConfig TestDiligentRenderBackend::MakeConfig() noexcept
{
    RenderBackendConfig config{};
    config.clearColor[0] = 0.0f;
    config.clearColor[1] = 0.0f;
    config.clearColor[2] = 0.0f;
    config.clearColor[3] = 1.0f;
#if defined(__linux__)
    config.preferredAPI = GraphicsBackendAPI::kNativePortable;
#endif
    return config;
}

void DiligentGPU::SetUp()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        GTEST_SKIP() << "SDL_Init(VIDEO) failed: " << SDL_GetError()
                     << " — no display available, skipping GPU tests.";
    }

    m_Window = SDL_CreateWindow(
        "SagaEngine_GPUTest",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(kWidth), static_cast<int>(kHeight),
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);

    // Fallback: if SDL_WINDOW_VULKAN fails, try without it (D3D11/GL).
    if (!m_Window)
    {
        m_Window = SDL_CreateWindow(
            "SagaEngine_GPUTest",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            static_cast<int>(kWidth), static_cast<int>(kHeight),
            SDL_WINDOW_HIDDEN);
    }

    if (!m_Window)
    {
        SDL_Quit();
        GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError()
                     << " — skipping GPU tests.";
    }

    void* native = GetNativeHandle(m_Window);
    if (!native)
    {
        SDL_DestroyWindow(m_Window);
        SDL_Quit();
        GTEST_SKIP() << "Could not extract native window handle"
                     << " — unsupported platform, skipping GPU tests.";
    }

    SwapchainDesc desc{};
    desc.nativeWindow = native;
    desc.width        = kWidth;
    desc.height       = kHeight;
    desc.vsync        = false;   // don't block on vblank in tests

    m_HasGPU = m_Backend.Initialize(desc);
    if (!m_HasGPU)
    {
        FAIL() << "DiligentRenderBackend::Initialize failed after SDL "
               << "created a native window; treating this as a GPU "
               << "backend regression instead of a skip.";
    }
}

void DiligentGPU::TearDown()
{
    if (m_HasGPU)
        m_Backend.Shutdown();

    if (m_Window)
        SDL_DestroyWindow(m_Window);

    SDL_Quit();
}

void DiligentGPU::PumpOneFrame()
{
    m_Backend.BeginFrame();
    Camera cam;
    RenderView view;
    m_Backend.Submit(cam, view);
    m_Backend.EndFrame();
}

RenderFrameCapture DiligentGPU::RenderAndCapture(
    const Camera& camera,
    const RenderView& view)
{
    RenderFrameCapture capture{};
    m_Backend.BeginFrame();
    m_Backend.Submit(camera, view);
    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();
    EXPECT_EQ(result, RenderCaptureResult::kSuccess);
    return capture;
}

SagaEngine::Render::World::MaterialId DiligentGPU::CreateSolidMaterial(
    std::uint64_t materialId,
    SagaTests::Render::Rgba8 color,
    SagaEngine::Render::TextureHandle* outTexture,
    SagaEngine::Render::OpaqueShadingModel shadingModel)
{
    const auto pixels = SagaTests::Render::SolidTexturePixels(color);
    const auto texture = m_Backend.CreateTexture(1u, 1u, pixels.data());
    EXPECT_NE(texture, SagaEngine::Render::TextureHandle::kInvalid);

    if (outTexture)
    {
        *outTexture = texture;
    }

    return m_Backend.CreateMaterial(
        SagaTests::Render::MakeMaterial(
            materialId,
            texture,
            SagaEngine::Render::MaterialCullMode::Back,
            shadingModel));
}

RenderView DiligentGPU::MakeSingleDrawView(
    SagaEngine::Render::World::MeshId mesh,
    SagaEngine::Render::World::MaterialId material,
    SagaEngine::Math::Mat4 model)
{
    RenderView view{};
    DrawItem item{};
    item.mesh = mesh;
    item.material = material;
    item.model = model;
    view.drawItems.push_back(item);
    return view;
}
