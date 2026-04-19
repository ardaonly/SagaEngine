/// @file RenderSubsystemIntegrationTests.cpp
/// @brief Multi-frame, scale, and fault-behaviour tests for the render driver.
///
/// Layer  : SagaEngine / Render
/// Purpose: Unit tests cover single-frame correctness. This file targets
///          the cross-frame invariants and ugly corners that only show up
///          under churn and scale:
///            - Long frame loops (counter integrity, no stat bleed).
///            - Many cameras (submit order + stats stay coherent).
///            - Rapid add/remove churn (slot recycling, no handle leaks).
///            - Entity churn interleaved with camera changes.
///            - Backend Submit throwing: documented propagation behaviour.
///            - LOD lookup invocation count stays sane.

#include "SagaEngine/Render/Backend/IRenderBackend.h"
#include "SagaEngine/Render/RenderSubsystem.h"
#include "SagaEngine/Render/Scene/FrameContext.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace SagaEngine::Render;
using SagaEngine::Math::Mat4;
using SagaEngine::Math::Vec3;

// ─── Helpers / doubles ────────────────────────────────────────────────

namespace
{

class CountingBackend final : public Backend::IRenderBackend
{
public:
    bool Initialize(const Backend::SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}
    World::MeshId     CreateMesh    (const MeshAsset&)        override { return static_cast<World::MeshId>(1); }
    World::MaterialId CreateMaterial(const MaterialRuntime&)  override { return static_cast<World::MaterialId>(1); }
    void              DestroyMesh    (World::MeshId)          override {}
    void              DestroyMaterial(World::MaterialId)      override {}
    TextureHandle     CreateTexture(uint32_t, uint32_t, const uint8_t*) override { return static_cast<TextureHandle>(1); }
    void              DestroyTexture(TextureHandle)           override {}
    void BeginFrame() override { ++begins; currentFrameSubmits = 0; }
    void Submit(const Scene::Camera&, const Scene::RenderView& view) override
    {
        ++submits;
        ++currentFrameSubmits;
        totalDrawItems += view.DrawCount();
    }
    void EndFrame() override { ++ends; submitsPerFrame.push_back(currentFrameSubmits); }

    std::size_t begins = 0, ends = 0, submits = 0;
    std::size_t totalDrawItems = 0;
    std::size_t currentFrameSubmits = 0;
    std::vector<std::size_t> submitsPerFrame;
};

/// Backend that throws on the N-th Submit call of its lifetime.
class ThrowingBackend final : public Backend::IRenderBackend
{
public:
    explicit ThrowingBackend(std::size_t throwOnNthSubmit)
        : m_TriggerAt(throwOnNthSubmit) {}

    bool Initialize(const Backend::SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}
    World::MeshId     CreateMesh    (const MeshAsset&)        override { return static_cast<World::MeshId>(1); }
    World::MaterialId CreateMaterial(const MaterialRuntime&)  override { return static_cast<World::MaterialId>(1); }
    void              DestroyMesh    (World::MeshId)          override {}
    void              DestroyMaterial(World::MaterialId)      override {}
    TextureHandle     CreateTexture(uint32_t, uint32_t, const uint8_t*) override { return static_cast<TextureHandle>(1); }
    void              DestroyTexture(TextureHandle)           override {}

    void BeginFrame() override { ++begins; }
    void Submit(const Scene::Camera&, const Scene::RenderView&) override
    {
        ++submits;
        if (submits == m_TriggerAt)
            throw std::runtime_error("synthetic backend failure");
    }
    void EndFrame() override { ++ends; }

    std::size_t begins = 0, ends = 0, submits = 0;
private:
    std::size_t m_TriggerAt = 0;
};

Scene::Camera OpenCamera(const Vec3& pos = Vec3{0, 0, 0})
{
    Scene::Camera cam{};
    cam.position = pos;
    cam.view       = Mat4::Identity();
    cam.projection = Mat4::Identity();
    // The subsystem will RecomputeFrustum each frame; pre-seeding wide
    // planes is harmless because RecomputeFrustum overwrites them.
    return cam;
}

World::RenderEntity MakeEntity(const Vec3& pos, float r = 0.05f)
{
    World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = r;
    e.visible            = true;
    return e;
}

} // namespace

// ─── Multi-frame loop ─────────────────────────────────────────────────

TEST(RenderSubsystemIntegration, SixtyFrameLoopKeepsCountersCoherent)
{
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);

    const auto main0 = rs.AddCamera(CameraRole::Main, OpenCamera());
    (void)main0;

    // Seed one entity at origin (inside unit-cube frustum).
    (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    for (std::uint64_t i = 0; i < 60; ++i)
    {
        Scene::FrameContext ctx;
        ctx.frameIndex   = i;
        ctx.deltaTimeSec = 1.0f / 60.0f;
        rs.ExecuteFrame(ctx);
    }

    EXPECT_EQ(backend.begins,  60u);
    EXPECT_EQ(backend.ends,    60u);
    EXPECT_EQ(backend.submits, 60u);
    // Every frame submitted exactly one camera.
    for (std::size_t n : backend.submitsPerFrame)
        EXPECT_EQ(n, 1u);

    // Last frame's aggregate stats must reflect THIS frame only, not the
    // cumulative total over 60 frames.
    EXPECT_EQ(rs.LastFrameStats().cameraCount, 1u);
    EXPECT_EQ(rs.LastFrameStats().totalDrawn,  1u);
    EXPECT_EQ(rs.LastFrameStats().totalVisited, 1u);
}

TEST(RenderSubsystemIntegration, FrameContextPassesThroughUnmodified)
{
    // FrameContext is observational today — this test fixes the fact so
    // a future refactor that starts wiring it into cameras doesn't break
    // callers silently.
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);
    (void)rs.AddCamera(CameraRole::Main, OpenCamera());

    Scene::FrameContext ctx;
    ctx.frameIndex      = 123456;
    ctx.deltaTimeSec    = 0.0166f;
    ctx.absoluteTimeSec = 999.5;

    rs.ExecuteFrame(ctx);
    // No observable side-effect today; successful run is the signal.
    EXPECT_EQ(backend.begins, 1u);
}

// ─── Scale ────────────────────────────────────────────────────────────

TEST(RenderSubsystemIntegration, TwoHundredFiftySixCameraStress)
{
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);

    // Populate 100 entities on layer 0x01.
    for (int i = 0; i < 100; ++i)
        (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

    // 256 cameras spread evenly across roles.
    std::vector<Scene::CameraId> ids;
    ids.reserve(256);
    for (int i = 0; i < 256; ++i)
    {
        const CameraRole role =
            (i % 4 == 0) ? CameraRole::Shadow  :
            (i % 4 == 1) ? CameraRole::Main    :
            (i % 4 == 2) ? CameraRole::Minimap :
                            CameraRole::Debug;
        ids.push_back(rs.AddCamera(role,
                                    OpenCamera(Vec3{static_cast<float>(i), 0, 0})));
    }
    EXPECT_EQ(rs.CameraCount(),        256u);
    EXPECT_EQ(rs.EnabledCameraCount(), 256u);

    // Submit order must be strictly non-decreasing by role.
    const auto order = rs.SubmitOrder();
    ASSERT_EQ(order.size(), 256u);
    for (std::size_t i = 1; i < order.size(); ++i)
    {
        const auto prev = rs.RoleOf(order[i - 1]);
        const auto cur  = rs.RoleOf(order[i]);
        EXPECT_LE(static_cast<int>(prev), static_cast<int>(cur));
    }

    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(backend.submits, 256u);
    // 100 entities × 256 cameras = 25 600 visited.
    EXPECT_EQ(rs.LastFrameStats().totalVisited, 25'600u);
}

// ─── Churn ────────────────────────────────────────────────────────────

TEST(RenderSubsystemIntegration, RapidCameraChurnDoesNotLeakSlots)
{
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);

    // 100 frames: in each, add 10 cameras, execute, remove all 10.
    for (int frame = 0; frame < 100; ++frame)
    {
        std::vector<Scene::CameraId> tmp;
        for (int i = 0; i < 10; ++i)
            tmp.push_back(rs.AddCamera(CameraRole::Debug, OpenCamera()));
        EXPECT_EQ(rs.CameraCount(), 10u);

        rs.ExecuteFrame(Scene::FrameContext{});

        for (auto id : tmp)
            EXPECT_TRUE(rs.RemoveCamera(id));
        EXPECT_EQ(rs.CameraCount(), 0u);
    }

    EXPECT_EQ(backend.begins, 100u);
    EXPECT_EQ(backend.submits, 100u * 10u);
}

TEST(RenderSubsystemIntegration, InterleavedEntityAndCameraChurnProducesExpectedDraws)
{
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);
    (void)rs.AddCamera(CameraRole::Main, OpenCamera());

    // Frame 1: 3 entities.
    std::vector<World::RenderEntityId> ents;
    for (int i = 0; i < 3; ++i)
        ents.push_back(world.Create(MakeEntity(Vec3{0, 0, 0})));
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().totalDrawn, 3u);

    // Frame 2: remove one, add two.
    world.Destroy(ents[1]);
    ents.push_back(world.Create(MakeEntity(Vec3{0, 0, 0})));
    ents.push_back(world.Create(MakeEntity(Vec3{0, 0, 0})));
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().totalDrawn, 4u);

    // Frame 3: add a second camera mid-session.
    const auto dbg = rs.AddCamera(CameraRole::Debug, OpenCamera(Vec3{99, 0, 0}));
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().cameraCount, 2u);
    EXPECT_EQ(rs.LastFrameStats().totalDrawn, 8u);  // 4 × 2 cameras.

    // Frame 4: remove the debug camera.
    rs.RemoveCamera(dbg);
    rs.ExecuteFrame(Scene::FrameContext{});
    EXPECT_EQ(rs.LastFrameStats().cameraCount, 1u);
    EXPECT_EQ(rs.LastFrameStats().totalDrawn, 4u);
}

// ─── LOD lookup call count ────────────────────────────────────────────

TEST(RenderSubsystemIntegration, LodLookupIsInvokedOncePerAutoEntityPerCamera)
{
    World::RenderWorld world;
    CountingBackend    backend;
    RenderSubsystem    rs(world, backend);

    std::atomic<std::size_t> calls{0};
    rs.SetLodLookup([&](World::MeshId) {
        calls.fetch_add(1, std::memory_order_relaxed);
        LOD::LodThresholds t{};
        t.byLevel = {0.0f, 10.0f, 100.0f, 1000.0f};
        t.count   = 4;
        return t;
    });

    // 5 entities: 3 on auto, 2 with explicit LOD override.
    for (int i = 0; i < 3; ++i)
        (void)world.Create(MakeEntity(Vec3{0, 0, 0}));
    for (int i = 0; i < 2; ++i)
    {
        auto e = MakeEntity(Vec3{0, 0, 0});
        e.lodOverride = 1;
        (void)world.Create(e);
    }

    // 2 cameras — each culls all 5 entities.
    (void)rs.AddCamera(CameraRole::Main,  OpenCamera());
    (void)rs.AddCamera(CameraRole::Debug, OpenCamera());

    rs.ExecuteFrame(Scene::FrameContext{});
    // 3 auto entities × 2 cameras = 6 lookup calls.
    // The 2 overridden entities must NOT invoke the lookup.
    EXPECT_EQ(calls.load(), 6u);
}

// ─── Backend fault propagation ────────────────────────────────────────

TEST(RenderSubsystemIntegration, BackendSubmitThrowPropagatesOut)
{
    // The subsystem does not swallow backend exceptions — this test
    // documents that. If a future version adds a catch, update this to
    // reflect the new contract explicitly.
    World::RenderWorld world;
    ThrowingBackend    backend(/*throwOnNthSubmit*/ 2);
    RenderSubsystem    rs(world, backend);

    const auto a = rs.AddCamera(CameraRole::Shadow, OpenCamera());
    const auto b = rs.AddCamera(CameraRole::Main,   OpenCamera());
    const auto c = rs.AddCamera(CameraRole::Debug,  OpenCamera());
    (void)a; (void)b; (void)c;

    bool threw = false;
    try {
        rs.ExecuteFrame(Scene::FrameContext{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    EXPECT_TRUE(threw);

    // Two cameras were submitted before the throw; the third never was.
    EXPECT_EQ(backend.submits, 2u);
    // BeginFrame ran; EndFrame did NOT (the throw skipped it).
    EXPECT_EQ(backend.begins,  1u);
    EXPECT_EQ(backend.ends,    0u);
}

// ─── Deterministic replay ─────────────────────────────────────────────

TEST(RenderSubsystemIntegration, IdenticalSetupProducesIdenticalSubmitSequence)
{
    auto runOnce = [](std::vector<Vec3>& outCameraOrder, std::vector<std::size_t>& outDraws)
    {
        World::RenderWorld world;
        CountingBackend    backend;
        RenderSubsystem    rs(world, backend);

        for (int i = 0; i < 7; ++i)
            (void)world.Create(MakeEntity(Vec3{0, 0, 0}));

        // Mixed roles in non-sorted insertion order.
        (void)rs.AddCamera(CameraRole::Main,    OpenCamera(Vec3{2, 0, 0}));
        (void)rs.AddCamera(CameraRole::Shadow,  OpenCamera(Vec3{1, 0, 0}));
        (void)rs.AddCamera(CameraRole::Debug,   OpenCamera(Vec3{4, 0, 0}));
        (void)rs.AddCamera(CameraRole::Minimap, OpenCamera(Vec3{3, 0, 0}));

        // Capture per-camera submit order + draw counts via a custom backend.
        struct Tap : Backend::IRenderBackend
        {
            std::vector<Vec3>& order;
            std::vector<std::size_t>& draws;
            Tap(std::vector<Vec3>& o, std::vector<std::size_t>& d) : order(o), draws(d) {}
            bool Initialize(const Backend::SwapchainDesc&) override { return true; }
            void Shutdown() override {}
            void OnResize(std::uint32_t, std::uint32_t) override {}
            World::MeshId     CreateMesh    (const MeshAsset&)        override { return static_cast<World::MeshId>(1); }
            World::MaterialId CreateMaterial(const MaterialRuntime&)  override { return static_cast<World::MaterialId>(1); }
            void              DestroyMesh    (World::MeshId)          override {}
            void              DestroyMaterial(World::MaterialId)      override {}
            TextureHandle     CreateTexture(uint32_t, uint32_t, const uint8_t*) override { return static_cast<TextureHandle>(1); }
            void              DestroyTexture(TextureHandle)           override {}
            void BeginFrame() override {}
            void Submit(const Scene::Camera& cam, const Scene::RenderView& v) override
            { order.push_back(cam.position); draws.push_back(v.DrawCount()); }
            void EndFrame() override {}
        };
        Tap tap(outCameraOrder, outDraws);
        RenderSubsystem rs2(world, tap);
        (void)rs2.AddCamera(CameraRole::Main,    OpenCamera(Vec3{2, 0, 0}));
        (void)rs2.AddCamera(CameraRole::Shadow,  OpenCamera(Vec3{1, 0, 0}));
        (void)rs2.AddCamera(CameraRole::Debug,   OpenCamera(Vec3{4, 0, 0}));
        (void)rs2.AddCamera(CameraRole::Minimap, OpenCamera(Vec3{3, 0, 0}));
        rs2.ExecuteFrame(Scene::FrameContext{});
    };

    std::vector<Vec3>        a, b;
    std::vector<std::size_t> da, db;
    runOnce(a, da);
    runOnce(b, db);

    ASSERT_EQ(a.size(), b.size());
    ASSERT_EQ(a.size(), 4u);
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        EXPECT_FLOAT_EQ(a[i].x, b[i].x);
        EXPECT_EQ(da[i],        db[i]);
    }
    // Expected submit order by role: Shadow(1), Main(2), Minimap(3), Debug(4).
    EXPECT_FLOAT_EQ(a[0].x, 1.0f);
    EXPECT_FLOAT_EQ(a[1].x, 2.0f);
    EXPECT_FLOAT_EQ(a[2].x, 3.0f);
    EXPECT_FLOAT_EQ(a[3].x, 4.0f);
}
