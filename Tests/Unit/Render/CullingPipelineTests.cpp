/// @file CullingPipelineTests.cpp
/// @brief Integration tests for the four-stage culling pipeline.
///
/// Layer  : SagaEngine / Render / Culling
/// Purpose: Verifies the stage ordering (hidden → visibility → distance →
///          frustum), per-stage counters, LOD override vs. auto lookup,
///          and visibility-mask intersection semantics. Uses a hand-
///          assembled "everything inside" frustum so stage interactions
///          can be isolated one filter at a time.

#include "SagaEngine/Render/Culling/CullingPipeline.h"
#include "SagaEngine/Render/LOD/LODSelector.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <gtest/gtest.h>

using namespace SagaEngine::Render;
using SagaEngine::Math::Vec3;

namespace
{

/// Return a camera whose frustum is effectively infinite — every one of
/// the six planes says "inside" for any finite point. Lets a test isolate
/// non-frustum stages.
Scene::Camera MakeOpenCamera(const Vec3& pos = Vec3{0, 0, 0})
{
    Scene::Camera cam{};
    cam.position        = pos;
    cam.maxDrawDistance = 0.0f;      // disabled.
    cam.filter          = World::kVisibilityAll;
    for (auto& p : cam.frustum.planes)
    {
        p.normal = Vec3{0, 0, 1};    // Arbitrary direction; large d swamps it.
        p.d      = 1e9f;
    }
    return cam;
}

World::RenderEntity Entity(const Vec3& pos, float r = 1.0f,
                             World::VisibilityMask mask = World::kVisibilityAll)
{
    World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = r;
    e.visible            = true;
    e.visibilityMask     = mask;
    return e;
}

} // namespace

// ─── Hidden flag ──────────────────────────────────────────────────────

TEST(Culling, HiddenEntitiesAreCulledFirst)
{
    World::RenderWorld w;
    const auto a = w.Create(Entity(Vec3{0, 0, 0}));
    const auto b = w.Create(Entity(Vec3{0, 0, 0}));
    w.SetVisible(a, false);
    (void)b;

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, MakeOpenCamera(), Culling::CullingConfig{}, view);

    EXPECT_EQ(view.visited,       2u);
    EXPECT_EQ(view.culledHidden,  1u);
    EXPECT_EQ(view.DrawCount(),   1u);
}

// ─── Visibility mask ──────────────────────────────────────────────────

TEST(Culling, VisibilityMaskIntersectionDecidesDraw)
{
    World::RenderWorld w;
    const auto layer1 = w.Create(Entity(Vec3{0,0,0}, 1.0f, /*mask*/0x01u));
    const auto layer2 = w.Create(Entity(Vec3{0,0,0}, 1.0f, /*mask*/0x02u));
    (void)layer1; (void)layer2;

    auto cam = MakeOpenCamera();
    cam.filter = 0x01u;   // Only layer 1.

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, cam, Culling::CullingConfig{}, view);

    EXPECT_EQ(view.DrawCount(),       1u);
    EXPECT_EQ(view.culledVisibility,  1u);
}

TEST(Culling, MultipleBitsInMaskAllowMultipleLayers)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{0,0,0}, 1.0f, /*mask*/0x01u));
    (void)w.Create(Entity(Vec3{0,0,0}, 1.0f, /*mask*/0x02u));
    (void)w.Create(Entity(Vec3{0,0,0}, 1.0f, /*mask*/0x04u));

    auto cam = MakeOpenCamera();
    cam.filter = 0x03u;  // layers 1 and 2.

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, cam, Culling::CullingConfig{}, view);

    EXPECT_EQ(view.DrawCount(),      2u);
    EXPECT_EQ(view.culledVisibility, 1u);
}

// ─── Distance culling ─────────────────────────────────────────────────

TEST(Culling, DistanceCullApplied)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{  10, 0, 0}));
    (void)w.Create(Entity(Vec3{  50, 0, 0}));
    (void)w.Create(Entity(Vec3{5000, 0, 0}));

    auto cam = MakeOpenCamera();
    cam.maxDrawDistance = 100.0f;

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, cam, Culling::CullingConfig{}, view);

    EXPECT_EQ(view.DrawCount(),     2u);
    EXPECT_EQ(view.culledDistance,  1u);
}

TEST(Culling, DistanceZeroDisablesMaxCheck)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{1e6f, 0, 0}));
    auto cam = MakeOpenCamera();
    cam.maxDrawDistance = 0.0f;

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, cam, Culling::CullingConfig{}, view);
    EXPECT_EQ(view.DrawCount(),    1u);
    EXPECT_EQ(view.culledDistance, 0u);
}

// ─── Frustum stage ────────────────────────────────────────────────────

TEST(Culling, FrustumRejectsEntitiesOutside)
{
    // Identity-projection unit-cube frustum → everything beyond ±1 gets
    // culled by the frustum stage.
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{ 0.0f, 0, 0}, 0.1f));
    (void)w.Create(Entity(Vec3{ 2.0f, 0, 0}, 0.1f));  // outside right.
    (void)w.Create(Entity(Vec3{-2.0f, 0, 0}, 0.1f));  // outside left.

    Scene::Camera cam{};
    cam.view       = SagaEngine::Math::Mat4::Identity();
    cam.projection = SagaEngine::Math::Mat4::Identity();
    cam.RecomputeViewProj();
    cam.RecomputeFrustum();

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, cam, Culling::CullingConfig{}, view);

    EXPECT_EQ(view.DrawCount(),    1u);
    EXPECT_EQ(view.culledFrustum,  2u);
}

// ─── LOD emission ─────────────────────────────────────────────────────

TEST(Culling, LodOverrideSkipsAutoLookup)
{
    World::RenderWorld w;
    const auto id = w.Create(Entity(Vec3{0, 0, 0}));
    w.SetLodOverride(id, 2u);

    Culling::CullingPipeline p;
    p.SetLodLookup([](World::MeshId) {
        // Should NOT be called when an override is set.
        ADD_FAILURE() << "LOD lookup unexpectedly invoked";
        return LOD::LodThresholds{};
    });
    Scene::RenderView view;
    p.Run(w, MakeOpenCamera(), Culling::CullingConfig{}, view);

    ASSERT_EQ(view.DrawCount(), 1u);
    EXPECT_EQ(view.drawItems[0].lod, 2u);
}

TEST(Culling, LodAutoUsesLookupFn)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{100.0f, 0, 0}));  // 100 units from origin.

    Culling::CullingPipeline p;
    p.SetLodLookup([](World::MeshId) {
        LOD::LodThresholds t{};
        t.byLevel = {0.0f, 20.0f, 80.0f, 200.0f};
        t.count   = 4;
        return t;
    });

    Scene::RenderView view;
    Culling::CullingConfig cfg;
    cfg.quality = LOD::QualityPreset::High;
    p.Run(w, MakeOpenCamera(), cfg, view);

    ASSERT_EQ(view.DrawCount(), 1u);
    // distance 100 → LOD 2 (80 <= 100 < 200).
    EXPECT_EQ(view.drawItems[0].lod, 2u);
}

TEST(Culling, AutoWithoutLookupDefaultsToLod0)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{500, 0, 0}));

    Culling::CullingPipeline p;   // No lookup installed.
    Scene::RenderView view;
    p.Run(w, MakeOpenCamera(), Culling::CullingConfig{}, view);

    ASSERT_EQ(view.DrawCount(), 1u);
    EXPECT_EQ(view.drawItems[0].lod, 0u);
}

// ─── RenderView reset between runs ────────────────────────────────────

TEST(Culling, ResetClearsPreviousFrameOutput)
{
    World::RenderWorld w;
    (void)w.Create(Entity(Vec3{0, 0, 0}));

    Culling::CullingPipeline p;
    Scene::RenderView view;
    p.Run(w, MakeOpenCamera(), Culling::CullingConfig{}, view);
    EXPECT_EQ(view.DrawCount(), 1u);

    // Empty the world; pipeline must reset the view and counters.
    w.Destroy(static_cast<World::RenderEntityId>(0));
    p.Run(w, MakeOpenCamera(), Culling::CullingConfig{}, view);
    EXPECT_EQ(view.DrawCount(), 0u);
    EXPECT_EQ(view.visited,      0u);
    EXPECT_EQ(view.culledHidden, 0u);
}
