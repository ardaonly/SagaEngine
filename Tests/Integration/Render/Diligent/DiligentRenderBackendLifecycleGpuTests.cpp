#include "DiligentGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

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

