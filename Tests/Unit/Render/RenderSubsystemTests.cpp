/// @file RenderSubsystemTests.cpp
/// @brief Multi-camera frame driver tests: isolation, ordering, stats,
///        lifecycle, and backend submit coupling.
///
/// Layer  : SagaEngine / Render
/// Purpose: Locks down the five invariants that distinguish a correct
///          multi-camera driver from a broken one:
///            1. Per-camera state isolation (lodBias / filter / frustum).
///            2. Per-camera RenderView (no cross-frame or cross-camera reuse).
///            3. Deterministic submit order (Shadow → Main → Minimap → Debug).
///            4. Aggregate stats match the sum of per-camera counters.
///            5. Disabled / removed cameras are not submitted.

#include "SagaEngine/Render/Backend/IRenderBackend.h"
#include "SagaEngine/Render/RenderSubsystem.h"
#include "SagaEngine/Render/Scene/FrameContext.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <gtest/gtest.h>

#include <vector>

using namespace SagaEngine::Render;
using SagaEngine::Math::Mat4;
using SagaEngine::Math::Vec3;

// ─── Test doubles ─────────────────────────────────────────────────────

namespace
{

/// Backend that records every Submit call so tests can assert on the
/// exact sequence the subsystem produced.
class RecordingBackend final : public Backend::IRenderBackend
{
public:
    struct Record
    {
        Scene::CameraId id        = Scene::CameraId::kInvalid;
        Vec3            cameraPos{};
        float           lodBias   = 0.0f;
        std::uint32_t   filter    = 0;
        std::size_t     drawCount = 0;
        std::uint32_t   visited   = 0;
    };

    bool Initialize(const Backend::SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}

    World::MeshId     CreateMesh(const MeshAsset&) override
    { return static_cast<World::MeshId>(1); }
    World::MaterialId CreateMaterial(const MaterialRuntime&) override
    { return static_cast<World::MaterialId>(1); }
    void              DestroyMesh    (World::MeshId)         override {}
    void              DestroyMaterial(World::MaterialId)     override {}
    TextureHandle     CreateTexture(uint32_t, uint32_t, const uint8_t*) override
    { return static_cast<TextureHandle>(1); }
    void              DestroyTexture(TextureHandle)          override {}

    void BeginFrame() override { ++frames; submits.clear(); }
    void Submit(const Scene::Camera& cam,
                 const Scene::RenderView& view) override
    {
        Record r;
        r.cameraPos = cam.position;
        r.lodBias   = cam.lodBias;
        r.filter    = cam.filter;
        r.drawCount = view.DrawCount();
        r.visited   = view.visited;
        // Camera id is not carried on Camera itself; tests match by
        // unique cameraPos values chosen to be distinct.
        submits.push_back(r);
    }
    void EndFrame() override { ++endFrames; }

    std::size_t            frames    = 0;
    std::size_t            endFrames = 0;
    std::vector<Record>    submits;
};

World::RenderEntity MakeEntity(const Vec3& pos,
                                 World::VisibilityMask mask = World::kVisibilityAll)
{
    World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = 1.0f;
    e.visible            = true;
    e.visibilityMask     = mask;
    return e;
}

/// Camera with an effectively infinite frustum; all finite points count
/// as inside so tests can isolate other stages. A unique `position` is
/// used as an identifier inside RecordingBackend::Record.
Scene::Camera OpenCamera(const Vec3& pos,
                          World::VisibilityMask filter = World::kVisibilityAll,
                          float                  lodBias = 1.0f)
{
    Scene::Camera cam{};
    cam.position = pos;
    cam.filter   = filter;
    cam.lodBias  = lodBias;
    cam.view       = Mat4::Identity();
    cam.projection = Mat4::Identity();
    cam.maxDrawDistance = 0.0f;
    // We bypass the subsystem's RecomputeFrustum effects by setting a
    // wide-open frustum directly. The subsystem calls RecomputeFrustum
    // each frame, so we seed view+proj with identity (→ unit cube) and
    // stamp wide planes right after AddCamera so culling passes through.
    return cam;
}

void OpenFrustumFor(Scene::Camera& cam) noexcept
{
    for (auto& p : cam.frustum.planes)
    {
        p.normal = Vec3{0, 0, 1};
        p.d      = 1e9f;
    }
}

} // namespace

// ─── Camera lifecycle ─────────────────────────────────────────────────

TEST(RenderSubsystem, AddAndRemoveCameraBookkeeping)
{
    World::RenderWorld       world;
    RecordingBackend         backend;
    RenderSubsystem          rs(world, backend);

    const auto a = rs.AddCamera(CameraRole::Main,   OpenCamera(Vec3{1, 0, 0}), "main");
    const auto b = rs.AddCamera(CameraRole::Debug,  OpenCamera(Vec3{2, 0, 0}), "debug");
    EXPECT_NE(a, b);
    EXPECT_EQ(rs.CameraCount(), 2u);
    EXPECT_EQ(rs.EnabledCameraCount(), 2u);
    EXPECT_EQ(rs.RoleOf(a), CameraRole::Main);
    EXPECT_EQ(rs.RoleOf(b), CameraRole::Debug);

    EXPECT_TRUE(rs.RemoveCamera(a));
    EXPECT_EQ(rs.CameraCount(), 1u);
    EXPECT_EQ(rs.GetCamera(a), nullptr);
    EXPECT_FALSE(rs.RemoveCamera(a));    // already gone.

    // Next AddCamera reuses the freed slot.
    const auto c = rs.AddCamera(CameraRole::Main, OpenCamera(Vec3{3, 0, 0}), "main2");
    EXPECT_EQ(static_cast<std::uint32_t>(c),
              static_cast<std::uint32_t>(a));
    EXPECT_EQ(rs.CameraCount(), 2u);
}

TEST(RenderSubsystem, DisabledCameraIsRetainedButNotSubmitted)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    const auto a = rs.AddCamera(CameraRole::Main,  OpenCamera(Vec3{1, 0, 0}));
    const auto b = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{2, 0, 0}));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    EXPECT_TRUE(rs.SetCameraEnabled(a, false));
    EXPECT_FALSE(rs.IsEnabled(a));
    EXPECT_EQ(rs.EnabledCameraCount(), 1u);

    Scene::FrameContext ctx; ctx.frameIndex = 1;
    rs.ExecuteFrame(ctx);

    ASSERT_EQ(backend.submits.size(), 1u);
    EXPECT_FLOAT_EQ(backend.submits[0].cameraPos.x, 2.0f);  // only debug.

    // Re-enable and run again.
    rs.SetCameraEnabled(a, true);
    ctx.frameIndex = 2;
    rs.ExecuteFrame(ctx);
    EXPECT_EQ(backend.submits.size(), 2u);
}

// ─── Deterministic submit order ───────────────────────────────────────

TEST(RenderSubsystem, SubmitOrderIsShadowThenMainThenMinimapThenDebug)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    // Insert cameras OUT OF role order on purpose — the subsystem must
    // reorder them at submit time.
    const auto debug   = rs.AddCamera(CameraRole::Debug,   OpenCamera(Vec3{4, 0, 0}));
    const auto main0   = rs.AddCamera(CameraRole::Main,    OpenCamera(Vec3{2, 0, 0}));
    const auto shadow  = rs.AddCamera(CameraRole::Shadow,  OpenCamera(Vec3{1, 0, 0}));
    const auto minimap = rs.AddCamera(CameraRole::Minimap, OpenCamera(Vec3{3, 0, 0}));
    for (auto id : {debug, main0, shadow, minimap})
        OpenFrustumFor(*rs.GetCamera(id));

    const auto order = rs.SubmitOrder();
    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], shadow);
    EXPECT_EQ(order[1], main0);
    EXPECT_EQ(order[2], minimap);
    EXPECT_EQ(order[3], debug);

    rs.ExecuteFrame(Scene::FrameContext{});
    ASSERT_EQ(backend.submits.size(), 4u);
    EXPECT_FLOAT_EQ(backend.submits[0].cameraPos.x, 1.0f);
    EXPECT_FLOAT_EQ(backend.submits[1].cameraPos.x, 2.0f);
    EXPECT_FLOAT_EQ(backend.submits[2].cameraPos.x, 3.0f);
    EXPECT_FLOAT_EQ(backend.submits[3].cameraPos.x, 4.0f);
}

TEST(RenderSubsystem, InsertionOrderIsStableWithinRole)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    const auto s0 = rs.AddCamera(CameraRole::Shadow, OpenCamera(Vec3{10, 0, 0}));
    const auto s1 = rs.AddCamera(CameraRole::Shadow, OpenCamera(Vec3{11, 0, 0}));
    const auto s2 = rs.AddCamera(CameraRole::Shadow, OpenCamera(Vec3{12, 0, 0}));
    for (auto id : {s0, s1, s2}) OpenFrustumFor(*rs.GetCamera(id));

    const auto order = rs.SubmitOrder();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], s0);
    EXPECT_EQ(order[1], s1);
    EXPECT_EQ(order[2], s2);
}

// ─── Camera isolation ─────────────────────────────────────────────────

TEST(RenderSubsystem, EachCameraSubmitsItsOwnLodBiasAndFilter)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    const auto a = rs.AddCamera(CameraRole::Main,
                                 OpenCamera(Vec3{1, 0, 0}, /*filter*/0x01u, /*lodBias*/0.5f));
    const auto b = rs.AddCamera(CameraRole::Debug,
                                 OpenCamera(Vec3{2, 0, 0}, /*filter*/0x02u, /*lodBias*/2.0f));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    rs.ExecuteFrame(Scene::FrameContext{});

    ASSERT_EQ(backend.submits.size(), 2u);
    EXPECT_FLOAT_EQ(backend.submits[0].lodBias, 0.5f);
    EXPECT_EQ      (backend.submits[0].filter, 0x01u);
    EXPECT_FLOAT_EQ(backend.submits[1].lodBias, 2.0f);
    EXPECT_EQ      (backend.submits[1].filter, 0x02u);
}

TEST(RenderSubsystem, ChangingOneCameraLodBiasDoesNotLeakToOthers)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    const auto a = rs.AddCamera(CameraRole::Main,  OpenCamera(Vec3{1, 0, 0}));
    const auto b = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{2, 0, 0}));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    rs.GetCamera(a)->lodBias = 0.25f;
    rs.ExecuteFrame(Scene::FrameContext{});

    ASSERT_EQ(backend.submits.size(), 2u);
    EXPECT_FLOAT_EQ(backend.submits[0].lodBias, 0.25f);  // main.
    EXPECT_FLOAT_EQ(backend.submits[1].lodBias, 1.0f);    // debug, untouched.
}

TEST(RenderSubsystem, PerCameraVisibilityFilterDecidesDrawsIndependently)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);

    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x01u));
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x02u));

    const auto a = rs.AddCamera(CameraRole::Main,  OpenCamera(Vec3{1, 0, 0}, 0x01u));
    const auto b = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{2, 0, 0}, 0x02u));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    rs.ExecuteFrame(Scene::FrameContext{});
    ASSERT_EQ(backend.submits.size(), 2u);
    EXPECT_EQ(backend.submits[0].drawCount, 1u);
    EXPECT_EQ(backend.submits[1].drawCount, 1u);
}

// ─── Per-camera view (no cross-camera reuse) ──────────────────────────

TEST(RenderSubsystem, LastViewsAreDistinctPerCamera)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x01u));
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x01u));
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x02u));

    const auto a = rs.AddCamera(CameraRole::Main,  OpenCamera(Vec3{1, 0, 0}, 0x01u));
    const auto b = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{2, 0, 0}, 0x02u));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    rs.ExecuteFrame(Scene::FrameContext{});

    const auto* va = rs.LastViewFor(a);
    const auto* vb = rs.LastViewFor(b);
    ASSERT_NE(va, nullptr);
    ASSERT_NE(vb, nullptr);
    EXPECT_NE(va, vb);                     // distinct storage.
    EXPECT_EQ(va->DrawCount(), 2u);
    EXPECT_EQ(vb->DrawCount(), 1u);
}

TEST(RenderSubsystem, ViewIsResetBetweenFrames)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    const auto a = rs.AddCamera(CameraRole::Main, OpenCamera(Vec3{1, 0, 0}));
    OpenFrustumFor(*rs.GetCamera(a));

    // Frame 1: two entities.
    const auto e0 = world.Create(MakeEntity(Vec3{0, 0, 0}));
    const auto e1 = world.Create(MakeEntity(Vec3{0, 0, 0}));
    (void)e1;
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastViewFor(a)->DrawCount(), 2u);

    // Frame 2: one entity removed — previous draw item must not survive.
    world.Destroy(e0);
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastViewFor(a)->DrawCount(), 1u);
}

// ─── Aggregate stats ──────────────────────────────────────────────────

TEST(RenderSubsystem, AggregateStatsEqualSumOfPerCameraCounters)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);

    // 3 entities: 2 on layer 1, 1 on layer 2.
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x01u));
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x01u));
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}, /*mask*/0x02u));

    const auto a = rs.AddCamera(CameraRole::Main,  OpenCamera(Vec3{1, 0, 0}, 0x01u));
    const auto b = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{2, 0, 0}, 0x03u));
    OpenFrustumFor(*rs.GetCamera(a));
    OpenFrustumFor(*rs.GetCamera(b));

    rs.ExecuteFrame(Scene::FrameContext{});

    const auto& s = rs.LastFrameStats();
    EXPECT_EQ(s.cameraCount, 2u);
    // Each camera visits all 3 entities — total 6.
    EXPECT_EQ(s.totalVisited, 6u);
    // a draws 2 (layer 1), b draws 3 (layers 1 and 2) — total 5.
    EXPECT_EQ(s.totalDrawn, 5u);
    // a culls 1 (layer 2), b culls 0 — total 1 visibility cull.
    EXPECT_EQ(s.totalCulledVisibility, 1u);
}

TEST(RenderSubsystem, StatsResetAcrossFrames)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));
    const auto a = rs.AddCamera(CameraRole::Main, OpenCamera(Vec3{1, 0, 0}));
    OpenFrustumFor(*rs.GetCamera(a));

    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().totalVisited, 1u);

    // Next frame with no entities should zero stats out.
    world.Destroy(static_cast<World::RenderEntityId>(0));
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().totalVisited, 0u);
    EXPECT_EQ(rs.LastFrameStats().totalDrawn,   0u);
    EXPECT_EQ(rs.LastFrameStats().cameraCount,  1u);
}

// ─── Backend lifecycle ────────────────────────────────────────────────

TEST(RenderSubsystem, BackendSeesBeginAndEndFrameEvenWithZeroCameras)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);

    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(backend.frames,    1u);
    EXPECT_EQ(backend.endFrames, 1u);
    EXPECT_EQ(backend.submits.size(), 0u);
    EXPECT_EQ(rs.LastFrameStats().cameraCount, 0u);
}

TEST(RenderSubsystem, SubsystemCallsRecomputeFrustumEachFrame)
{
    World::RenderWorld world;
    RecordingBackend   backend;
    RenderSubsystem    rs(world, backend);

    // Small bounds radius so the conservative sphere test does not save
    // an entity whose centre sits just outside a tightened frustum.
    auto tiny = [](const Vec3& p) {
        auto e = MakeEntity(p);
        e.boundsRadius = 0.05f;
        return e;
    };

    // Seed an entity at x=0 (safely inside the default unit-cube frustum).
    (void)world.Create(tiny(Vec3{0, 0, 0}));

    const auto a = rs.AddCamera(CameraRole::Main, OpenCamera(Vec3{0, 0, 0}));
    // Default Mat4::Identity projection → unit cube frustum after Recompute.
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(backend.submits.back().drawCount, 1u);

    // Scale the projection by 2 on each axis → frustum shrinks to
    // [-0.5, 0.5]^3. An entity at x=0.8 with tiny bounds must drop out.
    // The subsystem is responsible for calling RecomputeFrustum(); we
    // deliberately DO NOT call it ourselves.
    world.Destroy(static_cast<World::RenderEntityId>(0));
    (void)world.Create(tiny(Vec3{0.8f, 0, 0}));

    Mat4 scale = Mat4::Identity();
    scale.data[0]  = 2.0f;
    scale.data[5]  = 2.0f;
    scale.data[10] = 2.0f;
    rs.GetCamera(a)->projection = scale;

    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(backend.submits.back().drawCount, 0u);
}
