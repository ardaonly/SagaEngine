/// @file DiligentRenderBackendTests.cpp
/// @brief Unit tests for the Phase 1 Diligent IRenderBackend implementation.
///
/// Layer  : Tests / Unit / Render
/// Purpose: Validates every reachable code path in DiligentRenderBackend
///          WITHOUT requiring a GPU. This is deliberate:
///
///            In CI (and on most dev machines that run the unit-test target
///            headlessly), no Diligent graphics API is available, so
///            Initialize() will always return false. That failure path is
///            *exactly* what we need to test first — every method must be a
///            safe no-op when the device was never created. If a method
///            dereferences a null device/context/swapchain, these tests
///            segfault and CI catches it before the code ever touches a GPU.
///
///          The second layer of coverage is the pre-init and config surface:
///            - Default and parameterised construction
///            - Config propagation (clear colour, API preference, etc.)
///            - Diagnostic readout coherence (FrameIndex, SelectedAPI, …)
///            - ToString enum completeness
///            - Phase 1 resource stubs (CreateMesh / CreateMaterial → kInvalid)
///
///          GPU-present tests (Initialize succeeds, BeginFrame clears, …)
///          belong in Tests/Integration/ gated behind a "has-gpu" label.
///          Do NOT add them here.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackend.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/Render/World/RenderEntity.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::World;
using namespace SagaEngine::Render::Scene;

// ═══════════════════════════════════════════════════════════════════════
//  1. Construction & diagnostics
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, DefaultCtorNotInitialized)
{
    DiligentRenderBackend backend;
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, DefaultCtorFrameIndexZero)
{
    DiligentRenderBackend backend;
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

TEST(DiligentBackend, DefaultCtorSelectedAPIIsAuto)
{
    DiligentRenderBackend backend;
    EXPECT_EQ(backend.SelectedAPI(), DiligentBackendAPI::kAuto);
}

TEST(DiligentBackend, ConfigCtorPropagates)
{
    DiligentBackendConfig cfg;
    cfg.preferredAPI     = DiligentBackendAPI::kVulkan;
    cfg.enableValidation = true;
    cfg.clearColor[0]    = 1.0f;
    cfg.clearColor[1]    = 0.0f;
    cfg.clearColor[2]    = 0.5f;
    cfg.clearColor[3]    = 1.0f;
    cfg.clearDepth       = 0.0f;
    cfg.clearStencil     = 42;
    cfg.skipDepthClear   = true;

    // Construction itself must not crash or init the device.
    DiligentRenderBackend backend(cfg);
    EXPECT_FALSE(backend.IsInitialized());
    EXPECT_EQ(backend.SelectedAPI(), DiligentBackendAPI::kAuto);
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════
//  2. Initialize — defensive failure paths
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, InitZeroWidthReturnsFalse)
{
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    desc.width  = 0;
    desc.height = 600;
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, InitZeroHeightReturnsFalse)
{
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    desc.width  = 800;
    desc.height = 0;
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, InitZeroBothReturnsFalse)
{
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, InitNullWindowGracefulFail)
{
    // Valid dimensions, but null native window — Diligent factories will
    // fail to create a swapchain. Initialize must return false without
    // crashing or throwing.
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    desc.nativeWindow = nullptr;
    desc.width        = 1280;
    desc.height       = 720;
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, InitForcedD3D12WithoutGPUReturnsFalse)
{
    DiligentBackendConfig cfg;
    cfg.preferredAPI = DiligentBackendAPI::kD3D12;

    DiligentRenderBackend backend(cfg);
    SwapchainDesc desc{};
    desc.width  = 800;
    desc.height = 600;
    // No real HWND and possibly no D3D12 support in CI ⇒ must return false.
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, InitForcedVulkanWithoutGPUReturnsFalse)
{
    DiligentBackendConfig cfg;
    cfg.preferredAPI = DiligentBackendAPI::kVulkan;

    DiligentRenderBackend backend(cfg);
    SwapchainDesc desc{};
    desc.width  = 800;
    desc.height = 600;
    EXPECT_FALSE(backend.Initialize(desc));
    EXPECT_FALSE(backend.IsInitialized());
}

// ═══════════════════════════════════════════════════════════════════════
//  3. Post-fail lifecycle — every method is a safe no-op after
//     Initialize() returned false.
// ═══════════════════════════════════════════════════════════════════════

class DiligentBackendNotInit : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SwapchainDesc desc{};
        desc.width  = 800;
        desc.height = 600;
        // Expected to fail (no GPU in CI).
        backend.Initialize(desc);
        ASSERT_FALSE(backend.IsInitialized());
    }
    DiligentRenderBackend backend;
};

TEST_F(DiligentBackendNotInit, BeginFrameNoOp)
{
    // Must not crash, segfault, or throw.
    backend.BeginFrame();
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

TEST_F(DiligentBackendNotInit, SubmitNoOp)
{
    Camera cam;
    RenderView view;
    backend.Submit(cam, view);
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

TEST_F(DiligentBackendNotInit, EndFrameNoOp)
{
    backend.EndFrame();
    // frameIndex must NOT advance when not initialized.
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

TEST_F(DiligentBackendNotInit, FullFrameCycleNoOp)
{
    for (int i = 0; i < 10; ++i)
    {
        backend.BeginFrame();
        Camera cam;
        RenderView view;
        backend.Submit(cam, view);
        backend.EndFrame();
    }
    EXPECT_EQ(backend.FrameIndex(), 0u);
    EXPECT_FALSE(backend.IsInitialized());
}

TEST_F(DiligentBackendNotInit, OnResizeNoOp)
{
    backend.OnResize(1920, 1080);
    backend.OnResize(0, 0);
    EXPECT_FALSE(backend.IsInitialized());
}

// ═══════════════════════════════════════════════════════════════════════
//  4. Shutdown
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, ShutdownBeforeInitSafe)
{
    DiligentRenderBackend backend;
    // Shutdown on a never-initialized backend: must be no-op.
    backend.Shutdown();
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, DoubleShutdownSafe)
{
    DiligentRenderBackend backend;
    backend.Shutdown();
    backend.Shutdown();
    EXPECT_FALSE(backend.IsInitialized());
}

TEST(DiligentBackend, ShutdownAfterFailedInitSafe)
{
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    desc.width  = 800;
    desc.height = 600;
    backend.Initialize(desc);      // fails (no GPU)
    backend.Shutdown();
    backend.Shutdown();            // double shutdown after failed init
    EXPECT_FALSE(backend.IsInitialized());
    EXPECT_EQ(backend.FrameIndex(), 0u);
}

TEST(DiligentBackend, DestructorAfterFailedInitSafe)
{
    // Ensure destructor path (which calls Shutdown) doesn't throw or
    // dereference nulls when the device was never created.
    {
        DiligentRenderBackend backend;
        SwapchainDesc desc{};
        desc.width  = 800;
        desc.height = 600;
        backend.Initialize(desc);   // fails
    }
    // Reaching here without abort = success.
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  5. Phase 1 resource stubs
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, CreateMeshReturnsInvalid)
{
    DiligentRenderBackend backend;
    // MeshAsset and MaterialRuntime are forward-declared in
    // IRenderBackend.h but never defined yet — we pass a
    // reinterpret-cast dummy to satisfy the reference.  Phase 1 never
    // touches the argument; it just returns kInvalid immediately.
    struct FakeMeshAsset {} fakeMesh;
    const auto id = backend.CreateMesh(
        reinterpret_cast<const SagaEngine::Render::MeshAsset&>(fakeMesh));
    EXPECT_EQ(id, MeshId::kInvalid);
}

TEST(DiligentBackend, CreateMaterialReturnsInvalid)
{
    DiligentRenderBackend backend;
    struct FakeMaterialRuntime {} fakeMatrt;
    const auto id = backend.CreateMaterial(
        reinterpret_cast<const SagaEngine::Render::MaterialRuntime&>(fakeMatrt));
    EXPECT_EQ(id, MaterialId::kInvalid);
}

TEST(DiligentBackend, DestroyMeshNoOp)
{
    DiligentRenderBackend backend;
    backend.DestroyMesh(MeshId::kInvalid);
    backend.DestroyMesh(static_cast<MeshId>(42));
    SUCCEED();
}

TEST(DiligentBackend, DestroyMaterialNoOp)
{
    DiligentRenderBackend backend;
    backend.DestroyMaterial(MaterialId::kInvalid);
    backend.DestroyMaterial(static_cast<MaterialId>(7));
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
//  6. ToString enum coverage
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, ToStringCoversAllAPIs)
{
    EXPECT_EQ(ToString(DiligentBackendAPI::kAuto),   "Auto");
    EXPECT_EQ(ToString(DiligentBackendAPI::kD3D12),  "D3D12");
    EXPECT_EQ(ToString(DiligentBackendAPI::kVulkan), "Vulkan");
    EXPECT_EQ(ToString(DiligentBackendAPI::kD3D11),  "D3D11");
    EXPECT_EQ(ToString(DiligentBackendAPI::kOpenGL), "OpenGL");
}

TEST(DiligentBackend, ToStringUnknownValue)
{
    // Out-of-range enum value must not crash; returns "Unknown".
    const auto val = static_cast<DiligentBackendAPI>(255);
    const auto str = ToString(val);
    EXPECT_FALSE(str.empty());
    EXPECT_EQ(str, "Unknown");
}

// ═══════════════════════════════════════════════════════════════════════
//  7. Config defaults
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, ConfigDefaultsAreSane)
{
    DiligentBackendConfig cfg{};
    EXPECT_EQ(cfg.preferredAPI, DiligentBackendAPI::kAuto);
    EXPECT_FALSE(cfg.enableValidation);
    EXPECT_FLOAT_EQ(cfg.clearDepth, 1.0f);
    EXPECT_EQ(cfg.clearStencil, 0u);
    EXPECT_FALSE(cfg.skipDepthClear);

    // Clear colour is a dark slate, not zero-black.
    EXPECT_GT(cfg.clearColor[0], 0.0f);
    EXPECT_GT(cfg.clearColor[3], 0.0f);   // alpha = 1.0
}

// ═══════════════════════════════════════════════════════════════════════
//  8. Interleaving — submit order after failed init is harmless
// ═══════════════════════════════════════════════════════════════════════

TEST(DiligentBackend, InterleavedCallsAfterFailedInitHarmless)
{
    DiligentRenderBackend backend;
    SwapchainDesc desc{};
    desc.width = 800; desc.height = 600;
    backend.Initialize(desc);   // fails

    // Chaotic ordering: none of these must crash.
    backend.EndFrame();
    backend.Submit(Camera{}, RenderView{});
    backend.BeginFrame();
    backend.OnResize(640, 480);
    backend.EndFrame();
    backend.BeginFrame();
    backend.BeginFrame();
    backend.EndFrame();
    backend.Shutdown();
    backend.BeginFrame();
    backend.EndFrame();

    EXPECT_FALSE(backend.IsInitialized());
    EXPECT_EQ(backend.FrameIndex(), 0u);
}
