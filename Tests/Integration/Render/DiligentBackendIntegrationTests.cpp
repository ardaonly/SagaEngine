/// @file DiligentBackendIntegrationTests.cpp
/// @brief GPU-present integration tests for DiligentRenderBackend.
///
/// Layer  : Tests / Integration / Render
/// Purpose: Verifies the real Diligent device/swapchain lifecycle on a
///          machine that has a GPU + driver installed. Every test in this
///          file sits behind a GTEST_SKIP guard: if the GPU probe fails
///          (headless CI, missing driver, remote VM, etc.), the entire
///          suite is SKIPPED — not failed. This is the same pattern the
///          Persistence tests use for PostgreSQL availability.
///
/// Requires: SDL2::SDL2 linked to the test target.
///           See SagaTests.cmake — add `SDL2::SDL2` to
///           SagaIntegrationTests if not already present.
///
/// What is tested:
///   1. Device + context + swapchain creation via hidden SDL window
///   2. SelectedAPI is a concrete value after successful init
///   3. BeginFrame → EndFrame advances FrameIndex
///   4. Multi-frame pump (N frames, deterministic counter)
///   5. Submit with empty view (no crash)
///   6. OnResize to new valid dimensions
///   7. OnResize to zero (minimize guard)
///   8. Shutdown + re-Initialize round-trip
///   9. API fallback order verification (Auto picks *something*)
///  10. Clear-present cycle (no validation-layer errors)
///
/// What is NOT tested here:
///   - Mesh / material upload (Phase 2)
///   - Multi-threaded submission (Phase 2+)
///   - Rendering correctness (pixel readback, Phase 3+)

#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Materials/Material.h"
#include "SagaEngine/Render/Materials/MeshAsset.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "Render/DiligentPixelTestHelpers.h"

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

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

// ═══════════════════════════════════════════════════════════════════════
//  SDL + GPU fixture
// ═══════════════════════════════════════════════════════════════════════
//
// SetUp:
//   1. SDL_Init(VIDEO)
//   2. Create a tiny hidden window (no user interaction, no flicker)
//   3. Extract native window handle → SwapchainDesc
//   4. Attempt DiligentRenderBackend::Initialize
//   5. If Initialize fails → GTEST_SKIP (not FAIL)
//
// TearDown:
//   1. backend.Shutdown()
//   2. SDL_DestroyWindow
//   3. SDL_Quit

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

SagaEngine::Render::MeshAsset BuildIndexedCubeMeshAsset()
{
    SagaEngine::Render::MeshAsset asset{};
    asset.meshId = 1;
    asset.debugName = "DiligentGPUIndexedCube";
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};

    auto& lod = asset.lods[0];

    auto pushFace = [&](SagaEngine::Render::MeshVec3 p0,
                        SagaEngine::Render::MeshVec3 p1,
                        SagaEngine::Render::MeshVec3 p2,
                        SagaEngine::Render::MeshVec3 p3,
                        SagaEngine::Render::MeshVec3 normal,
                        SagaEngine::Render::MeshVec3 tangent)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVertex = [&](SagaEngine::Render::MeshVec3 position,
                              SagaEngine::Render::MeshVec2 uv)
        {
            SagaEngine::Render::MeshVertex vertex{};
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.handedness = 1.0f;
            vertex.uv0 = uv;
            return vertex;
        };

        lod.vertices.push_back(makeVertex(p0, {0.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p1, {1.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p2, {1.0f, 0.0f}));
        lod.vertices.push_back(makeVertex(p3, {0.0f, 0.0f}));

        lod.indices.push_back(base);
        lod.indices.push_back(base + 1u);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base + 3u);
    };

    constexpr float h = 0.5f;
    pushFace({-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h},
             {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h},
             {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f});
    pushFace({h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h},
             {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
    pushFace({-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h},
             {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    pushFace({-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h},
             {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h},
             {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    lod.approxGpuBytes =
        static_cast<std::uint64_t>(lod.vertices.size()) *
            sizeof(SagaEngine::Render::MeshVertex) +
        static_cast<std::uint64_t>(lod.indices.size()) *
            sizeof(std::uint32_t);

    return asset;
}

SagaEngine::Render::Scene::Camera MakeIndexedCubeCamera()
{
    SagaEngine::Render::Scene::Camera camera{};
    camera.position = {0.0f, 0.0f, 3.0f};
    camera.view = SagaEngine::Math::Mat4::LookAtRH(
        camera.position,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f});
    camera.projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    return camera;
}

class TestDiligentRenderBackend
{
public:
    TestDiligentRenderBackend()
        : m_backend(MakeConfig())
    {
    }

    [[nodiscard]] bool Initialize(const SwapchainDesc& desc)
    {
        return m_backend.Initialize(desc);
    }

    void Shutdown()
    {
        m_backend.Shutdown();
    }

    void OnResize(std::uint32_t width, std::uint32_t height)
    {
        m_backend.OnResize(width, height);
    }

    [[nodiscard]] SagaEngine::Render::World::MeshId CreateMesh(
        const SagaEngine::Render::MeshAsset& asset)
    {
        return m_backend.CreateMesh(asset);
    }

    [[nodiscard]] SagaEngine::Render::World::MaterialId CreateMaterial(
        const SagaEngine::Render::MaterialRuntime& runtime)
    {
        return m_backend.CreateMaterial(runtime);
    }

    void DestroyMesh(SagaEngine::Render::World::MeshId id)
    {
        m_backend.DestroyMesh(id);
    }

    void DestroyMaterial(SagaEngine::Render::World::MaterialId id)
    {
        m_backend.DestroyMaterial(id);
    }

    [[nodiscard]] SagaEngine::Render::TextureHandle CreateTexture(
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* rgba)
    {
        return m_backend.CreateTexture(width, height, rgba);
    }

    void DestroyTexture(SagaEngine::Render::TextureHandle texture)
    {
        m_backend.DestroyTexture(texture);
    }

    void BeginFrame()
    {
        m_backend.BeginFrame();
    }

    void Submit(const Camera& camera, const RenderView& view)
    {
        m_backend.Submit(camera, view);
    }

    void EndFrame()
    {
        m_backend.EndFrame();
    }

    [[nodiscard]] GraphicsBackendAPI SelectedAPI() const noexcept
    {
        return m_backend.SelectedAPI();
    }

    [[nodiscard]] std::uint64_t FrameIndex() const noexcept
    {
        return m_backend.FrameIndex();
    }

    [[nodiscard]] bool IsInitialized() const noexcept
    {
        return m_backend.IsInitialized();
    }

    [[nodiscard]] RenderFrameDiagnostics LastFrameDiagnostics() const noexcept
    {
        return m_backend.LastFrameDiagnostics();
    }

    [[nodiscard]] RenderCaptureResult CaptureCurrentColorFrame(
        RenderFrameCapture& capture)
    {
        return m_backend.CaptureCurrentColorFrame(capture);
    }

private:
    static RenderBackendConfig MakeConfig() noexcept
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

    SagaEngine::Render::Backend::DiligentRenderBackend m_backend;
};

} // namespace

class DiligentGPU : public ::testing::Test
{
protected:
    SDL_Window*               m_Window  = nullptr;
    TestDiligentRenderBackend m_Backend;
    bool                      m_HasGPU  = false;

    static constexpr std::uint32_t kWidth  = 320;
    static constexpr std::uint32_t kHeight = 240;

    void SetUp() override
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

    void TearDown() override
    {
        if (m_HasGPU)
            m_Backend.Shutdown();

        if (m_Window)
            SDL_DestroyWindow(m_Window);

        SDL_Quit();
    }

    /// Helper: pump one full frame through the backend.
    void PumpOneFrame()
    {
        m_Backend.BeginFrame();
        Camera cam;
        RenderView view;
        m_Backend.Submit(cam, view);
        m_Backend.EndFrame();
    }

    [[nodiscard]] RenderFrameCapture RenderAndCapture(
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

    [[nodiscard]] SagaEngine::Render::World::MaterialId CreateSolidMaterial(
        std::uint64_t materialId,
        SagaTests::Render::Rgba8 color,
        SagaEngine::Render::TextureHandle* outTexture = nullptr,
        SagaEngine::Render::OpaqueShadingModel shadingModel =
            SagaEngine::Render::OpaqueShadingModel::Unlit)
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

    [[nodiscard]] RenderView MakeSingleDrawView(
        SagaEngine::Render::World::MeshId mesh,
        SagaEngine::Render::World::MaterialId material,
        SagaEngine::Math::Mat4 model = SagaEngine::Math::Mat4::Identity())
    {
        RenderView view{};
        DrawItem item{};
        item.mesh = mesh;
        item.material = material;
        item.model = model;
        view.drawItems.push_back(item);
        return view;
    }
};

// ═══════════════════════════════════════════════════════════════════════
//  1. Init verification
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, InitSucceeds)
{
    EXPECT_TRUE(m_Backend.IsInitialized());
}

TEST_F(DiligentGPU, SelectedAPIIsConcrete)
{
    // After successful init, SelectedAPI must be a real API, not kAuto.
    EXPECT_NE(m_Backend.SelectedAPI(), GraphicsBackendAPI::kAuto);
}

TEST_F(DiligentGPU, SelectedAPINameIsNotEmpty)
{
    const auto name = ToString(m_Backend.SelectedAPI());
    EXPECT_FALSE(name.empty());
    EXPECT_NE(name, "Auto");
    EXPECT_NE(name, "Unknown");
}

// ═══════════════════════════════════════════════════════════════════════
//  2. Frame lifecycle
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, SingleFrameAdvancesFrameIndex)
{
    EXPECT_EQ(m_Backend.FrameIndex(), 0u);
    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
}

TEST_F(DiligentGPU, MultiFramePump)
{
    constexpr std::uint64_t kFrames = 60;
    for (std::uint64_t i = 0; i < kFrames; ++i)
        PumpOneFrame();

    EXPECT_EQ(m_Backend.FrameIndex(), kFrames);
}

TEST_F(DiligentGPU, BeginEndWithoutSubmit)
{
    // Valid: BeginFrame → EndFrame with zero Submit calls.
    m_Backend.BeginFrame();
    m_Backend.EndFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
}

TEST_F(DiligentGPU, MultipleSubmitsPerFrame)
{
    // Valid: multiple cameras submit per frame.
    m_Backend.BeginFrame();
    Camera cam1, cam2, cam3;
    RenderView view1, view2, view3;
    m_Backend.Submit(cam1, view1);
    m_Backend.Submit(cam2, view2);
    m_Backend.Submit(cam3, view3);
    m_Backend.EndFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  3. Resize
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, ResizeToValidDimensions)
{
    // Resize, then pump a frame — must not crash or corrupt state.
    m_Backend.OnResize(640, 480);
    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
    EXPECT_TRUE(m_Backend.IsInitialized());
}

TEST_F(DiligentGPU, ResizeToZeroIsNoOp)
{
    // Simulates window minimize. Must not crash.
    m_Backend.OnResize(0, 0);
    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
}

TEST_F(DiligentGPU, ResizeBackToOriginal)
{
    m_Backend.OnResize(1280, 720);
    PumpOneFrame();
    m_Backend.OnResize(kWidth, kHeight);
    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 2u);
}

// ═══════════════════════════════════════════════════════════════════════
//  4. Shutdown + re-init round-trip
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, ShutdownAndReinitialize)
{
    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);

    m_Backend.Shutdown();
    EXPECT_FALSE(m_Backend.IsInitialized());
    EXPECT_EQ(m_Backend.SelectedAPI(), GraphicsBackendAPI::kAuto);

    // Re-initialize with the same window.
    void* native = GetNativeHandle(m_Window);
    ASSERT_NE(native, nullptr);

    SwapchainDesc desc{};
    desc.nativeWindow = native;
    desc.width        = kWidth;
    desc.height       = kHeight;
    desc.vsync        = false;

    EXPECT_TRUE(m_Backend.Initialize(desc));
    EXPECT_TRUE(m_Backend.IsInitialized());
    EXPECT_NE(m_Backend.SelectedAPI(), GraphicsBackendAPI::kAuto);

    // Frame counter resets on re-init.
    EXPECT_EQ(m_Backend.FrameIndex(), 0u);

    PumpOneFrame();
    EXPECT_EQ(m_Backend.FrameIndex(), 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  5. Double-init is no-op
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, DoubleInitIsNoOp)
{
    const auto apiBefore = m_Backend.SelectedAPI();

    void* native = GetNativeHandle(m_Window);
    SwapchainDesc desc{};
    desc.nativeWindow = native;
    desc.width        = kWidth;
    desc.height       = kHeight;

    // Second Initialize on an already-initialized backend → true, unchanged.
    EXPECT_TRUE(m_Backend.Initialize(desc));
    EXPECT_EQ(m_Backend.SelectedAPI(), apiBefore);
}

// ═══════════════════════════════════════════════════════════════════════
//  6. API fallback — kAuto picks something
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, AutoFallbackPicksSomething)
{
    // The fixture uses kAuto (default config). If we got here, init
    // succeeded, which means Auto resolved to a concrete API.
    const auto api = m_Backend.SelectedAPI();
    EXPECT_TRUE(api == GraphicsBackendAPI::kNativePrimary  ||
                api == GraphicsBackendAPI::kNativePortable  ||
                api == GraphicsBackendAPI::kNativeLegacy   ||
                api == GraphicsBackendAPI::kCompatibility);
}

// ═══════════════════════════════════════════════════════════════════════
//  7. Clear + present cycle (validation layer smoke test)
// ═══════════════════════════════════════════════════════════════════════
//
// If the Diligent validation layer is active, any misuse (wrong state
// transitions, null RTV, bad clear flags) triggers a debug break or
// logged error. Pumping multiple frames exercises the clear + present
// path enough to catch these at CI time.

TEST_F(DiligentGPU, HundredFrameStressNoCrash)
{
    for (int i = 0; i < 100; ++i)
        PumpOneFrame();

    EXPECT_EQ(m_Backend.FrameIndex(), 100u);
    EXPECT_TRUE(m_Backend.IsInitialized());
}

// ═══════════════════════════════════════════════════════════════════════
//  8. Resource upload with a live device
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, CreateMeshRejectsEmptyAsset)
{
    const SagaEngine::Render::MeshAsset emptyAsset{};
    const auto id = m_Backend.CreateMesh(emptyAsset);
    EXPECT_EQ(id, SagaEngine::Render::World::MeshId::kInvalid);
}

TEST_F(DiligentGPU, CreateMaterialReturnsValidWithLiveDevice)
{
    const SagaEngine::Render::MaterialRuntime material{};
    const auto id = m_Backend.CreateMaterial(material);
    EXPECT_NE(id, SagaEngine::Render::World::MaterialId::kInvalid);
}

TEST_F(DiligentGPU, IndexedCubeSubmitsAndPresents)
{
    const auto cube = BuildIndexedCubeMeshAsset();
    ASSERT_EQ(cube.lodCount, 1u);
    ASSERT_EQ(cube.lods[0].vertices.size(), 24u)
        << "deterministic indexed cube must provide 24 vertices";
    ASSERT_EQ(cube.lods[0].indices.size(), 36u)
        << "deterministic indexed cube must provide 36 indices";

    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid)
        << "mesh handle valid";

    SagaEngine::Render::MaterialRuntime material{};
    material.materialId = 1u;
    material.renderQueue = SagaEngine::Render::MaterialRenderQueue::Opaque;
    material.cullMode = SagaEngine::Render::MaterialCullMode::Back;
    material.writesDepth = true;

    const auto materialHandle = m_Backend.CreateMaterial(material);
    ASSERT_NE(materialHandle, SagaEngine::Render::World::MaterialId::kInvalid)
        << "material handle valid";

    const auto frameBefore = m_Backend.FrameIndex();

    RenderView view{};
    DrawItem item{};
    item.mesh = mesh;
    item.material = materialHandle;
    item.model = SagaEngine::Math::Mat4::Identity();
    view.drawItems.push_back(item);

    m_Backend.BeginFrame();
    m_Backend.Submit(MakeIndexedCubeCamera(), view);
    m_Backend.EndFrame();

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(m_Backend.FrameIndex(), frameBefore + 1u);
    EXPECT_EQ(diagnostics.submittedDrawItems, 1u)
        << "submittedDrawItems == 1";
    EXPECT_EQ(diagnostics.indexedDrawCalls, 1u)
        << "indexedDrawCalls == 1";
    EXPECT_EQ(diagnostics.nonIndexedDrawCalls, 0u)
        << "nonIndexedDrawCalls == 0";
    EXPECT_EQ(diagnostics.lastIndexedIndexCount, 36u)
        << "lastIndexedIndexCount == 36";
    EXPECT_EQ(diagnostics.presentCalls, 1u)
        << "presentCalls == 1";

    m_Backend.DestroyMaterial(materialHandle);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ClearFrameCanBeReadBack)
{
    m_Backend.BeginFrame();
    RenderFrameCapture capture{};
    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();

    ASSERT_EQ(result, RenderCaptureResult::kSuccess);
    ASSERT_EQ(capture.width, kWidth);
    ASSERT_EQ(capture.height, kHeight);
    ASSERT_EQ(capture.rowPitch, kWidth * 4u);
    ASSERT_EQ(capture.format, RenderPixelFormat::RGBA8_UNORM);
    ASSERT_EQ(capture.pixels.size(),
              static_cast<std::size_t>(kWidth) * kHeight * 4u);

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, kWidth / 2u, kHeight / 2u),
        kClear, 3));
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, 0u, 0u), kClear, 3));
}

TEST_F(DiligentGPU, IndexedCubeProducesNonClearPixels)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        10u, {40u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 500u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantBlue(c);
                  }),
              20u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.submittedDrawItems, 1u);
    EXPECT_EQ(diagnostics.indexedDrawCalls, 1u);
    EXPECT_EQ(diagnostics.presentCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, UploadedTextureAffectsRenderedPixels)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle redTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        11u, {255u, 20u, 20u, 255u}, &redTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 16u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              100u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(redTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, NearGeometryOccludesFarGeometry)
{
    const auto nearQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.1f, 0.5f, "NearDepthQuad");
    const auto farQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.1f, 0.0f, "FarDepthQuad");
    const auto nearMesh = m_Backend.CreateMesh(nearQuad);
    const auto farMesh = m_Backend.CreateMesh(farQuad);
    ASSERT_NE(nearMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(farMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle nearTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle farTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto nearMaterial = CreateSolidMaterial(
        12u, {30u, 255u, 30u, 255u}, &nearTex);
    const auto farMaterial = CreateSolidMaterial(
        13u, {255u, 30u, 30u, 255u}, &farTex);
    ASSERT_NE(nearMaterial, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(farMaterial, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem nearItem{};
    nearItem.mesh = nearMesh;
    nearItem.material = nearMaterial;
    view.drawItems.push_back(nearItem);
    DrawItem farItem{};
    farItem.mesh = farMesh;
    farItem.material = farMaterial;
    view.drawItems.push_back(farItem);

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              50u);
    EXPECT_LT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              5u);

    m_Backend.DestroyMaterial(nearMaterial);
    m_Backend.DestroyMaterial(farMaterial);
    m_Backend.DestroyTexture(nearTex);
    m_Backend.DestroyTexture(farTex);
    m_Backend.DestroyMesh(nearMesh);
    m_Backend.DestroyMesh(farMesh);
}

TEST_F(DiligentGPU, ModelTransformChangesScreenCoverage)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        14u, {60u, 90u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto camera = SagaTests::Render::MakeCamera();
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    const auto smallCapture = RenderAndCapture(
        camera,
        MakeSingleDrawView(
            mesh, material,
            SagaEngine::Math::Mat4::FromScale({0.55f, 0.55f, 0.55f})));
    const auto largeCapture = RenderAndCapture(
        camera,
        MakeSingleDrawView(
            mesh, material,
            SagaEngine::Math::Mat4::FromScale({1.25f, 1.25f, 1.25f})));

    const auto smallBounds =
        SagaTests::Render::FindNonClearBounds(smallCapture, kClear, 8);
    const auto largeBounds =
        SagaTests::Render::FindNonClearBounds(largeCapture, kClear, 8);

    ASSERT_TRUE(smallBounds.valid);
    ASSERT_TRUE(largeBounds.valid);
    EXPECT_GT(largeBounds.Area(), smallBounds.Area() * 2u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, IndexedCubeRendersAcrossMultipleFrames)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        15u, {40u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto camera = SagaTests::Render::MakeCamera();
    const auto view = MakeSingleDrawView(mesh, material);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};

    for (int i = 0; i < 3; ++i)
    {
        const auto capture = RenderAndCapture(camera, view);
        EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8),
                  500u);

        const auto diagnostics = m_Backend.LastFrameDiagnostics();
        EXPECT_EQ(diagnostics.submittedDrawItems, 1u);
        EXPECT_EQ(diagnostics.indexedDrawCalls, 1u);
        EXPECT_EQ(diagnostics.presentCalls, 1u);
    }

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ResizeContinuesProducingValidPixels)
{
    SDL_SetWindowSize(m_Window, 400, 300);
    m_Backend.OnResize(400u, 300u);

    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        16u, {50u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(400.0f / 300.0f),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(capture.width, 400u);
    EXPECT_EQ(capture.height, 300u);
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8),
              500u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, DestroyedResourcesAreRejectedBySubmission)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        17u, {50u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyMesh(mesh);
    m_Backend.DestroyTexture(blueTex);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.submittedDrawItems, 0u);
    EXPECT_EQ(diagnostics.rejectedDrawItems, 1u);
    EXPECT_EQ(diagnostics.indexedDrawCalls, 0u);
    EXPECT_EQ(diagnostics.presentCalls, 1u);
}

TEST_F(DiligentGPU, DirectionalLightChangesSurfaceLuminance)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        30u, {220u, 220u, 220u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto litView = MakeSingleDrawView(mesh, material);
    litView.lighting.directionalEnabled = true;
    litView.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    litView.lighting.directional.color = {1.0f, 1.0f, 1.0f};
    litView.lighting.directional.intensity = 1.0f;
    litView.lighting.ambient.intensity = 0.02f;

    auto darkView = litView;
    darkView.lighting.directional.direction = {0.0f, 0.0f, 1.0f};

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, litView);
    const auto darkCapture = RenderAndCapture(camera, darkView);

    const float litLum = SagaTests::Render::AverageRegionLuminance(
        litCapture, litCapture.width / 2u, litCapture.height / 2u, 18u);
    const float darkLum = SagaTests::Render::AverageRegionLuminance(
        darkCapture, darkCapture.width / 2u, darkCapture.height / 2u, 18u);
    EXPECT_GT(litLum, darkLum + 80.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, VertexNormalsAffectLighting)
{
    auto facingQuad = SagaTests::Render::BuildQuadMeshAsset(
        0.32f, 0.0f, "NormalFacingQuad");
    auto grazingQuad = SagaTests::Render::BuildQuadMeshAsset(
        0.32f, 0.0f, "NormalGrazingQuad");
    for (auto& vertex : grazingQuad.lods[0].vertices)
        vertex.normal = {1.0f, 0.0f, 0.0f};

    const auto facingMesh = m_Backend.CreateMesh(facingQuad);
    const auto grazingMesh = m_Backend.CreateMesh(grazingQuad);
    ASSERT_NE(facingMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(grazingMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        31u, {230u, 230u, 230u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem facing{};
    facing.mesh = facingMesh;
    facing.material = material;
    facing.model = SagaEngine::Math::Mat4::FromTranslation({-0.45f, 0.0f, 0.0f});
    view.drawItems.push_back(facing);

    DrawItem grazing{};
    grazing.mesh = grazingMesh;
    grazing.material = material;
    grazing.model = SagaEngine::Math::Mat4::FromTranslation({0.45f, 0.0f, 0.0f});
    view.drawItems.push_back(grazing);

    view.lighting.directionalEnabled = true;
    view.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    view.lighting.directional.color = {1.0f, 1.0f, 1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.04f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);

    const float facingLum = SagaTests::Render::AverageRegionLuminance(
        capture, capture.width / 2u - 36u, capture.height / 2u, 12u);
    const float grazingLum = SagaTests::Render::AverageRegionLuminance(
        capture, capture.width / 2u + 36u, capture.height / 2u, 12u);
    EXPECT_GT(facingLum, grazingLum + 80.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(facingMesh);
    m_Backend.DestroyMesh(grazingMesh);
}

TEST_F(DiligentGPU, AmbientLightKeepsUnlitFacingSurfaceVisible)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        32u, {200u, 200u, 200u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto noAmbientView = MakeSingleDrawView(mesh, material);
    noAmbientView.lighting.directionalEnabled = true;
    noAmbientView.lighting.directional.direction = {0.0f, 0.0f, 1.0f};
    noAmbientView.lighting.directional.intensity = 1.0f;
    noAmbientView.lighting.ambient.intensity = 0.0f;

    auto ambientView = noAmbientView;
    ambientView.lighting.ambient.color = {1.0f, 1.0f, 1.0f};
    ambientView.lighting.ambient.intensity = 0.35f;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto noAmbientCapture = RenderAndCapture(camera, noAmbientView);
    const auto ambientCapture = RenderAndCapture(camera, ambientView);

    const float noAmbientLum = SagaTests::Render::AverageRegionLuminance(
        noAmbientCapture, noAmbientCapture.width / 2u, noAmbientCapture.height / 2u, 18u);
    const float ambientLum = SagaTests::Render::AverageRegionLuminance(
        ambientCapture, ambientCapture.width / 2u, ambientCapture.height / 2u, 18u);
    EXPECT_GT(ambientLum, noAmbientLum + 45.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, UnlitMaterialIgnoresDirectionalLight)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        33u, {50u, 80u, 240u, 255u}, &blueTex,
        SagaEngine::Render::OpaqueShadingModel::Unlit);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto viewA = MakeSingleDrawView(mesh, material);
    viewA.lighting.directionalEnabled = true;
    viewA.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    viewA.lighting.directional.intensity = 2.0f;
    viewA.lighting.ambient.intensity = 0.0f;

    auto viewB = viewA;
    viewB.lighting.directional.direction = {0.0f, 0.0f, 1.0f};
    viewB.lighting.ambient.intensity = 1.0f;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto captureA = RenderAndCapture(camera, viewA);
    const auto captureB = RenderAndCapture(camera, viewB);

    const float lumA = SagaTests::Render::AverageRegionLuminance(
        captureA, captureA.width / 2u, captureA.height / 2u, 18u);
    const float lumB = SagaTests::Render::AverageRegionLuminance(
        captureB, captureB.width / 2u, captureB.height / 2u, 18u);
    EXPECT_NEAR(lumA, lumB, 4.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowPassSubmitsDepthDraws)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        40u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.2f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8), 500u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 1u);
    EXPECT_EQ(diagnostics.mainPassDrawCalls, 1u);
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowMapResourceIsPersistentAcrossFrames)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        41u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    auto first = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(first.shadowResourceCreationCount, 1u);
    EXPECT_EQ(first.shadowResourceRecreationCount, 0u);

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    auto second = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(second.shadowResourceCreationCount, 0u);
    EXPECT_EQ(second.shadowResourceRecreationCount, 0u);
    EXPECT_EQ(second.shadowPassExecuted, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowEnabledFrameRejectsAdditionalSubmit)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        42u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    m_Backend.BeginFrame();
    m_Backend.Submit(SagaTests::Render::MakeCamera(), view);
    m_Backend.Submit(SagaTests::Render::MakeCamera(), view);
    m_Backend.EndFrame();

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.additionalFrameSubmitsRejected, 1u);
    EXPECT_EQ(diagnostics.mainPassDrawCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowSamplingProducesValidatedPixels)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        43u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.2f, -0.4f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::AverageRegionLuminance(
                  capture, capture.width / 2u, capture.height / 2u, 18u),
              20.0f);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, OccluderCastsShadowOnReceiver)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiver"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluder"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle groundTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle cubeTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto groundMat = CreateSolidMaterial(
        44u, {210u, 210u, 210u, 255u}, &groundTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    const auto cubeMat = CreateSolidMaterial(
        45u, {180u, 180u, 180u, 255u}, &cubeTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(groundMat, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(cubeMat, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;

    DrawItem receiver{};
    receiver.mesh = receiverMesh;
    receiver.material = groundMat;
    view.drawItems.push_back(receiver);

    DrawItem occluder{};
    occluder.mesh = occluderMesh;
    occluder.material = cubeMat;
    occluder.model =
        SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
        SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
    view.drawItems.push_back(occluder);

    auto noShadowView = view;
    noShadowView.lighting.shadowsEnabled = false;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, noShadowView);
    const auto shadowCapture = RenderAndCapture(camera, view);
    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);

    EXPECT_GT(darker.count, 80u);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 2u);

    m_Backend.DestroyMaterial(groundMat);
    m_Backend.DestroyMaterial(cubeMat);
    m_Backend.DestroyTexture(groundTex);
    m_Backend.DestroyTexture(cubeTex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, DisablingShadowsRestoresReceiverLuminance)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverToggle"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderToggle"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle groundTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle cubeTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto groundMat = CreateSolidMaterial(
        46u, {210u, 210u, 210u, 255u}, &groundTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    const auto cubeMat = CreateSolidMaterial(
        146u, {180u, 180u, 180u, 255u}, &cubeTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(groundMat, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(cubeMat, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;
    DrawItem receiver{};
    receiver.mesh = receiverMesh;
    receiver.material = groundMat;
    view.drawItems.push_back(receiver);
    DrawItem occluder{};
    occluder.mesh = occluderMesh;
    occluder.material = cubeMat;
    occluder.model =
        SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
        SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
    view.drawItems.push_back(occluder);

    auto noShadowView = view;
    noShadowView.lighting.shadowsEnabled = false;
    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, noShadowView);
    const auto noShadowDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto shadowCapture = RenderAndCapture(camera, view);
    const auto shadowDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);
    EXPECT_GT(darker.count, 80u);
    EXPECT_EQ(noShadowDiagnostics.shadowPassExecuted, 0u);
    EXPECT_EQ(shadowDiagnostics.shadowPassExecuted, 1u);

    m_Backend.DestroyMaterial(groundMat);
    m_Backend.DestroyMaterial(cubeMat);
    m_Backend.DestroyTexture(groundTex);
    m_Backend.DestroyTexture(cubeTex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowResultIsIndependentOfMainPassDrawOrder)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverOrder"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderOrder"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        47u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool cubeFirst, bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});

        if (cubeFirst)
        {
            view.drawItems.push_back(occluder);
            view.drawItems.push_back(receiver);
        }
        else
        {
            view.drawItems.push_back(receiver);
            view.drawItems.push_back(occluder);
        }
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litA = RenderAndCapture(camera, makeView(false, false));
    const auto shadowA = RenderAndCapture(camera, makeView(false, true));
    const auto litB = RenderAndCapture(camera, makeView(true, false));
    const auto shadowB = RenderAndCapture(camera, makeView(true, true));
    const auto darkerA = SagaTests::Render::FindDarkerPixelCentroid(
        litA, shadowA, 18.0f);
    const auto darkerB = SagaTests::Render::FindDarkerPixelCentroid(
        litB, shadowB, 18.0f);
    EXPECT_GT(darkerA.count, 50u);
    EXPECT_GT(darkerB.count, 50u);
    EXPECT_NEAR(static_cast<float>(darkerA.count),
                static_cast<float>(darkerB.count), 1200.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, MovingOccluderMovesProjectedShadow)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.9f, 0.0f, "ShadowReceiverMoving"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderMoving"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        48u, {215u, 215u, 215u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](float x, bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;
        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);
        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({x, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto leftLit = RenderAndCapture(camera, makeView(-0.45f, false));
    const auto leftShadow = RenderAndCapture(camera, makeView(-0.45f, true));
    const auto rightLit = RenderAndCapture(camera, makeView(0.45f, false));
    const auto rightShadow = RenderAndCapture(camera, makeView(0.45f, true));

    const auto leftCentroid = SagaTests::Render::FindDarkerPixelCentroid(
        leftLit, leftShadow, 18.0f);
    const auto rightCentroid = SagaTests::Render::FindDarkerPixelCentroid(
        rightLit, rightShadow, 18.0f);
    EXPECT_GT(leftCentroid.count, 50u);
    EXPECT_GT(rightCentroid.count, 50u);
    EXPECT_GT(rightCentroid.x, leftCentroid.x + 20.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowMapOutsideProjectionRemainsLit)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        49u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.15f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::AverageRegionLuminance(
                  capture, capture.width / 2u, capture.height / 2u, 18u),
              120.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowPcfConfigPropagatesConfiguredResolution)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        50u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.2f, -0.4f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, LightingAndShadowsRemainValidAcrossMultipleFrames)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverMulti"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderMulti"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        51u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, makeView(false));

    for (int frame = 0; frame < 3; ++frame)
    {
        const auto shadowCapture = RenderAndCapture(camera, makeView(true));
        const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
            litCapture, shadowCapture, 18.0f);
        EXPECT_GT(darker.count, 80u);

        const auto diagnostics = m_Backend.LastFrameDiagnostics();
        EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
        EXPECT_EQ(diagnostics.shadowPassDrawCalls, 2u);
        EXPECT_EQ(diagnostics.mainPassDrawCalls, 2u);
        EXPECT_EQ(diagnostics.presentCalls, 1u);
        EXPECT_EQ(diagnostics.shadowResourceRecreationCount, 0u);
        if (frame == 0)
            EXPECT_EQ(diagnostics.shadowResourceCreationCount, 1u);
        else
            EXPECT_EQ(diagnostics.shadowResourceCreationCount, 0u);
    }

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ResizePreservesLightingAndShadows)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverResize"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderResize"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        52u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    (void)RenderAndCapture(camera, makeView(true));
    auto firstDiagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(firstDiagnostics.shadowResourceCreationCount, 1u);

    SDL_SetWindowSize(m_Window, 400, 300);
    m_Backend.OnResize(400u, 300u);

    const auto resizedCamera = SagaTests::Render::MakeCamera(400.0f / 300.0f);
    const auto litCapture = RenderAndCapture(resizedCamera, makeView(false));
    const auto shadowCapture = RenderAndCapture(resizedCamera, makeView(true));
    EXPECT_EQ(shadowCapture.width, 400u);
    EXPECT_EQ(shadowCapture.height, 300u);

    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);
    EXPECT_GT(darker.count, 80u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowResourceCreationCount, 0u);
    EXPECT_EQ(diagnostics.shadowResourceRecreationCount, 0u);
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowToggleDoesNotLeakOrUseStaleBindings)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverToggleStale"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderToggleStale"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        53u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto enabledA = RenderAndCapture(camera, makeView(true));
    (void)enabledA;
    const auto disabled = RenderAndCapture(camera, makeView(false));
    const auto disabledDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto enabledB = RenderAndCapture(camera, makeView(true));
    const auto enabledDiagnostics = m_Backend.LastFrameDiagnostics();

    const auto disabledVsShadowAgain = SagaTests::Render::FindDarkerPixelCentroid(
        disabled, enabledB, 18.0f);
    EXPECT_GT(disabledVsShadowAgain.count, 80u);
    EXPECT_EQ(disabledDiagnostics.shadowPassExecuted, 0u);
    EXPECT_EQ(disabledDiagnostics.shadowSamplingEnabled, 0u);
    EXPECT_EQ(enabledDiagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(enabledDiagnostics.shadowSamplingEnabled, 2u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}
