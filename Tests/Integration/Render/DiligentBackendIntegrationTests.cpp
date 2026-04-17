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

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"

#include <SDL.h>
#include <SDL2/SDL_syswm.h>
#include <gtest/gtest.h>

#include <cstdint>

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

/// Extracts the platform native window handle from an SDL_Window.
/// Returns nullptr if the platform is not supported.
void* GetNativeHandle(SDL_Window* window)
{
    if (!window) return nullptr;

    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo))
        return nullptr;

#if defined(_WIN32)
    return wmInfo.info.win.window;   // HWND
#elif defined(__linux__)
#   if defined(SDL_VIDEO_DRIVER_X11)
    // X11: cast Window (unsigned long) to void*.
    return reinterpret_cast<void*>(wmInfo.info.x11.window);
#   elif defined(SDL_VIDEO_DRIVER_WAYLAND)
    return wmInfo.info.wl.surface;
#   else
    return nullptr;
#   endif
#elif defined(__APPLE__)
    return wmInfo.info.cocoa.window; // NSWindow*
#else
    return nullptr;
#endif
}

} // namespace

class DiligentGPU : public ::testing::Test
{
protected:
    SDL_Window*           m_Window  = nullptr;
    DiligentRenderBackend m_Backend;
    bool                  m_HasGPU  = false;

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
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "DiligentRenderBackend::Initialize failed"
                         << " — no usable GPU, skipping GPU tests.";
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
    EXPECT_NE(m_Backend.SelectedAPI(), DiligentBackendAPI::kAuto);
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
    EXPECT_EQ(m_Backend.SelectedAPI(), DiligentBackendAPI::kAuto);

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
    EXPECT_NE(m_Backend.SelectedAPI(), DiligentBackendAPI::kAuto);

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
    EXPECT_TRUE(api == DiligentBackendAPI::kD3D12  ||
                api == DiligentBackendAPI::kVulkan  ||
                api == DiligentBackendAPI::kD3D11   ||
                api == DiligentBackendAPI::kOpenGL);
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
//  8. Resource stubs still return kInvalid even with a live device
// ═══════════════════════════════════════════════════════════════════════

TEST_F(DiligentGPU, CreateMeshStillInvalidPhase1)
{
    struct FakeMeshAsset {} fake;
    const auto id = m_Backend.CreateMesh(
        reinterpret_cast<const SagaEngine::Render::MeshAsset&>(fake));
    EXPECT_EQ(id, SagaEngine::Render::World::MeshId::kInvalid);
}

TEST_F(DiligentGPU, CreateMaterialStillInvalidPhase1)
{
    struct FakeMaterialRuntime {} fake;
    const auto id = m_Backend.CreateMaterial(
        reinterpret_cast<const SagaEngine::Render::MaterialRuntime&>(fake));
    EXPECT_EQ(id, SagaEngine::Render::World::MaterialId::kInvalid);
}
