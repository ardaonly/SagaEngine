/// @file RenderClientAppTests.cpp
/// @brief Lifecycle and wire-up tests for the client-side render facade.
///
/// Layer  : Runtime / Client
/// Purpose: Verifies that RenderClientApp composes RenderWorld,
///          NetEntityBinding, and RenderSubsystem correctly, honours the
///          DefaultCameras flags, forwards LOD lookup + quality preset
///          to the subsystem, and drives the backend through the right
///          sequence of Initialize / BeginFrame / Submit / EndFrame /
///          Shutdown calls.

#include "SagaRuntime/Client/RenderClientApp.h"
#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <gtest/gtest.h>

#include <atomic>
#include <vector>

namespace R = SagaEngine::Render;
namespace C = SagaEngine::Runtime::Client;
using SagaEngine::Math::Vec3;
using SagaEngine::Math::Mat4;

namespace
{

/// Backend that records the full lifecycle call sequence so tests can
/// assert on ordering, counts, and on swapchain-desc passthrough.
class TracingBackend final : public R::Backend::IRenderBackend
{
public:
    bool Initialize(const R::Backend::SwapchainDesc& d) override
    {
        ++initCount; lastDesc = d;
        return !failInit;
    }
    void Shutdown() override { ++shutdownCount; }
    void OnResize(std::uint32_t w, std::uint32_t h) override { lastResize = {w, h}; }
    R::World::MeshId     CreateMesh    (const R::MeshAsset&)        override { return static_cast<R::World::MeshId>(1); }
    R::World::MaterialId CreateMaterial(const R::MaterialRuntime&)  override { return static_cast<R::World::MaterialId>(1); }
    void                 DestroyMesh    (R::World::MeshId)          override {}
    void                 DestroyMaterial(R::World::MaterialId)      override {}
    R::TextureHandle     CreateTexture(uint32_t, uint32_t, const uint8_t*) override { return static_cast<R::TextureHandle>(1); }
    void                 DestroyTexture(R::TextureHandle)           override {}

    void BeginFrame() override { ++beginFrames; }
    void Submit(const R::Scene::Camera&, const R::Scene::RenderView& v) override
    { ++submits; lastDraws = v.DrawCount(); }
    void EndFrame() override { ++endFrames; }

    // State.
    bool failInit = false;
    R::Backend::SwapchainDesc      lastDesc{};
    std::pair<std::uint32_t,std::uint32_t> lastResize{};
    std::size_t initCount     = 0;
    std::size_t shutdownCount = 0;
    std::size_t beginFrames   = 0;
    std::size_t endFrames     = 0;
    std::size_t submits       = 0;
    std::size_t lastDraws     = 0;
};

C::RenderClientConfig BaseConfig()
{
    C::RenderClientConfig cfg;
    cfg.swapchain.width  = 800;
    cfg.swapchain.height = 600;
    cfg.swapchain.vsync  = true;
    return cfg;
}

R::World::RenderEntity MakeEnt(std::uint32_t netId, const Vec3& pos)
{
    R::World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = 0.05f;
    e.visible            = true;
    e.networkId          = netId;
    return e;
}

/// Seed a camera pair with identity view+proj so the subsystem's
/// RecomputeFrustum produces the unit cube and entities at origin draw.
void SeedCameraIdentityMatrices(C::RenderClientApp& app)
{
    for (auto id : {app.MainCameraId(), app.ShadowCameraId(),
                     app.MinimapCameraId(), app.DebugCameraId()})
    {
        if (id == R::Scene::CameraId::kInvalid) continue;
        if (auto* cam = app.Subsystem().GetCamera(id))
        {
            cam->view       = Mat4::Identity();
            cam->projection = Mat4::Identity();
        }
    }
}

} // namespace

// ─── Initialize / Shutdown ────────────────────────────────────────────

TEST(RenderClientApp, InitializeForwardsSwapchainDescToBackend)
{
    TracingBackend b;
    C::RenderClientApp app(b);

    auto cfg = BaseConfig();
    cfg.swapchain.width  = 1920;
    cfg.swapchain.height = 1080;
    cfg.swapchain.hdr    = true;

    ASSERT_TRUE(app.Initialize(cfg));
    EXPECT_EQ(b.initCount, 1u);
    EXPECT_EQ(b.lastDesc.width,  1920u);
    EXPECT_EQ(b.lastDesc.height, 1080u);
    EXPECT_TRUE(b.lastDesc.hdr);
    EXPECT_TRUE(app.IsInitialized());
}

TEST(RenderClientApp, InitializeFailureLeavesFacadeUnusable)
{
    TracingBackend b; b.failInit = true;
    C::RenderClientApp app(b);
    EXPECT_FALSE(app.Initialize(BaseConfig()));
    EXPECT_FALSE(app.IsInitialized());

    // Tick is a safe no-op before init.
    app.Tick(R::Scene::FrameContext{});
    EXPECT_EQ(b.beginFrames, 0u);
}

TEST(RenderClientApp, DoubleInitializeIsNoOp)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    ASSERT_TRUE(app.Initialize(BaseConfig()));
    ASSERT_TRUE(app.Initialize(BaseConfig()));
    EXPECT_EQ(b.initCount, 1u);
}

TEST(RenderClientApp, ShutdownIsIdempotent)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    ASSERT_TRUE(app.Initialize(BaseConfig()));

    app.Shutdown();
    EXPECT_EQ(b.shutdownCount, 1u);
    EXPECT_FALSE(app.IsInitialized());

    app.Shutdown();                // second call: no extra backend call.
    EXPECT_EQ(b.shutdownCount, 1u);
}

TEST(RenderClientApp, DestructorShutsBackendDown)
{
    TracingBackend b;
    {
        C::RenderClientApp app(b);
        ASSERT_TRUE(app.Initialize(BaseConfig()));
    }
    EXPECT_EQ(b.shutdownCount, 1u);
}

// ─── Default camera flags ─────────────────────────────────────────────

TEST(RenderClientApp, DefaultCamerasRespectFlags)
{
    TracingBackend b;
    C::RenderClientApp app(b);

    auto cfg = BaseConfig();
    cfg.defaultCameras.main    = true;
    cfg.defaultCameras.shadow  = true;
    cfg.defaultCameras.minimap = false;
    cfg.defaultCameras.debug   = true;

    ASSERT_TRUE(app.Initialize(cfg));
    EXPECT_NE(app.MainCameraId(),    R::Scene::CameraId::kInvalid);
    EXPECT_NE(app.ShadowCameraId(),  R::Scene::CameraId::kInvalid);
    EXPECT_EQ(app.MinimapCameraId(), R::Scene::CameraId::kInvalid);
    EXPECT_NE(app.DebugCameraId(),   R::Scene::CameraId::kInvalid);
    EXPECT_EQ(app.Subsystem().CameraCount(), 3u);
}

TEST(RenderClientApp, AllFlagsOffCreatesNoCameras)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    auto cfg = BaseConfig();
    cfg.defaultCameras = {};       // all false.
    cfg.defaultCameras.main = false;

    ASSERT_TRUE(app.Initialize(cfg));
    EXPECT_EQ(app.Subsystem().CameraCount(), 0u);
    EXPECT_EQ(app.MainCameraId(), R::Scene::CameraId::kInvalid);
}

// ─── LOD lookup + quality forwarding ──────────────────────────────────

TEST(RenderClientApp, ConfigForwardsLodLookupAndQuality)
{
    TracingBackend b;
    C::RenderClientApp app(b);

    std::atomic<std::size_t> lookupCalls{0};
    auto cfg = BaseConfig();
    cfg.quality = R::LOD::QualityPreset::Ultra;
    cfg.lodLookup = [&](R::World::MeshId) {
        lookupCalls.fetch_add(1, std::memory_order_relaxed);
        R::LOD::LodThresholds t{};
        t.byLevel = {0.0f, 10.0f, 100.0f, 1000.0f};
        t.count   = 4;
        return t;
    };
    ASSERT_TRUE(app.Initialize(cfg));
    EXPECT_EQ(app.Subsystem().QualityPreset(), R::LOD::QualityPreset::Ultra);

    SeedCameraIdentityMatrices(app);
    app.Bridge().BindOrCreate(1, MakeEnt(1, Vec3{0, 0, 0}));
    app.Tick(R::Scene::FrameContext{});
    EXPECT_GT(lookupCalls.load(), 0u);
}

// ─── End-to-end Tick → Submit ─────────────────────────────────────────

TEST(RenderClientApp, TickDrivesBackendInCorrectSequence)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    auto cfg = BaseConfig();
    cfg.defaultCameras.main   = true;
    cfg.defaultCameras.shadow = true;
    ASSERT_TRUE(app.Initialize(cfg));
    SeedCameraIdentityMatrices(app);

    app.Bridge().BindOrCreate(100, MakeEnt(100, Vec3{0, 0, 0}));
    app.Bridge().BindOrCreate(101, MakeEnt(101, Vec3{0, 0, 0}));

    app.Tick(R::Scene::FrameContext{});
    EXPECT_EQ(b.beginFrames, 1u);
    EXPECT_EQ(b.endFrames,   1u);
    // 2 default cameras × 1 submit each.
    EXPECT_EQ(b.submits, 2u);
    EXPECT_EQ(b.lastDraws, 2u);     // last camera saw both entities.

    // Multi-frame.
    for (int i = 0; i < 10; ++i)
        app.Tick(R::Scene::FrameContext{});
    EXPECT_EQ(b.beginFrames, 11u);
    EXPECT_EQ(b.endFrames,   11u);
    EXPECT_EQ(b.submits,     22u);  // 2 cams × 11 frames.
}

TEST(RenderClientApp, BridgeReleaseRemovesEntityFromNextFrame)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    auto cfg = BaseConfig();
    cfg.defaultCameras.main = true;
    ASSERT_TRUE(app.Initialize(cfg));
    SeedCameraIdentityMatrices(app);

    app.Bridge().BindOrCreate(1, MakeEnt(1, Vec3{0, 0, 0}));
    app.Tick(R::Scene::FrameContext{});
    EXPECT_EQ(b.lastDraws, 1u);

    app.Bridge().Release(1);
    app.Tick(R::Scene::FrameContext{});
    EXPECT_EQ(b.lastDraws, 0u);
}

TEST(RenderClientApp, ShutdownClearsBridgeMappings)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    ASSERT_TRUE(app.Initialize(BaseConfig()));
    app.Bridge().BindOrCreate(1, MakeEnt(1, Vec3{0, 0, 0}));
    app.Bridge().BindOrCreate(2, MakeEnt(2, Vec3{0, 0, 0}));
    EXPECT_EQ(app.Bridge().Size(), 2u);

    app.Shutdown();
    EXPECT_EQ(app.Bridge().Size(), 0u);
    EXPECT_EQ(app.World().LiveCount(), 0u);
}

TEST(RenderClientApp, TickWithoutInitializeIsSafeNoOp)
{
    TracingBackend b;
    C::RenderClientApp app(b);
    app.Tick(R::Scene::FrameContext{});      // no init → must not crash.
    EXPECT_EQ(b.beginFrames, 0u);
    EXPECT_EQ(b.submits,     0u);
}
