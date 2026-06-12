#include "SagaEngine/Render/Scene/RenderFrameSnapshot.h"

#include <gtest/gtest.h>

#include <string>

using namespace SagaEngine::Render;
using SagaEngine::Math::Mat4;
using SagaEngine::Math::Vec3;

namespace {

World::RenderEntity Entity(const Vec3& pos,
                           World::VisibilityMask mask = World::kVisibilityAll)
{
    World::RenderEntity entity;
    entity.transform.position = pos;
    entity.mesh = static_cast<World::MeshId>(1);
    entity.material = static_cast<World::MaterialId>(1);
    entity.boundsRadius = 0.05f;
    entity.visibilityMask = mask;
    entity.visible = true;
    return entity;
}

Scene::Camera CameraAt(const Vec3& pos = Vec3{0.0f, 0.0f, 0.0f},
                       World::VisibilityMask filter = World::kVisibilityAll)
{
    Scene::Camera camera;
    camera.position = pos;
    camera.view = Mat4::Identity();
    camera.projection = Mat4::Identity();
    camera.filter = filter;
    camera.maxDrawDistance = 0.0f;
    return camera;
}

Scene::RenderViewDescriptor View(Scene::RenderViewKind kind,
                                 std::uint32_t index,
                                 std::uint32_t order,
                                 Scene::Camera camera = CameraAt())
{
    Scene::RenderViewDescriptor view;
    view.kind = kind;
    view.viewIndex = index;
    view.stableOrder = order;
    view.camera = camera;
    view.debugName = "view-" + std::to_string(index);
    return view;
}

} // namespace

TEST(RenderSceneExtraction, ProducesDrawPacketsAndCullCounters)
{
    World::RenderWorld world;
    (void)world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}, 0x01u));
    (void)world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}, 0x02u));
    const auto hidden = world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}, 0x01u));
    world.SetVisible(hidden, false);

    auto farEntity = Entity(Vec3{5.0f, 0.0f, 0.0f}, 0x01u);
    farEntity.boundsRadius = 0.05f;
    (void)world.Create(farEntity);

    auto camera = CameraAt(Vec3{0.0f, 0.0f, 0.0f}, 0x01u);
    camera.maxDrawDistance = 2.0f;

    Scene::RenderViewFamily family;
    family.AddView(View(Scene::RenderViewKind::Main, 0, 1, camera));

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(
        world,
        family,
        Scene::FrameContext{.frameIndex = 7});

    EXPECT_EQ(snapshot.stats.viewCount, 1u);
    EXPECT_EQ(snapshot.stats.submittedViewCount, 1u);
    EXPECT_EQ(snapshot.stats.visited, 4u);
    EXPECT_EQ(snapshot.stats.packetCount, 1u);
    EXPECT_EQ(snapshot.stats.culledVisibility, 1u);
    EXPECT_EQ(snapshot.stats.culledHidden, 1u);
    EXPECT_EQ(snapshot.stats.culledDistance, 1u);
    ASSERT_EQ(snapshot.drawPackets.size(), 1u);
    EXPECT_EQ(snapshot.drawPackets[0].viewKind, Scene::RenderViewKind::Main);
}

TEST(RenderSceneExtraction, InvalidMeshAndMaterialProduceDiagnostics)
{
    World::RenderWorld world;
    auto missingMesh = Entity(Vec3{0.0f, 0.0f, 0.0f});
    missingMesh.mesh = World::MeshId::kInvalid;
    (void)world.Create(missingMesh);

    auto missingMaterial = Entity(Vec3{0.0f, 0.0f, 0.0f});
    missingMaterial.material = World::MaterialId::kInvalid;
    (void)world.Create(missingMaterial);

    Scene::RenderViewFamily family;
    family.AddView(View(Scene::RenderViewKind::Main, 0, 1));

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(
        world,
        family,
        {});

    EXPECT_EQ(snapshot.stats.packetCount, 0u);
    EXPECT_EQ(snapshot.stats.invalidMesh, 1u);
    EXPECT_EQ(snapshot.stats.invalidMaterial, 1u);
    ASSERT_EQ(snapshot.diagnostics.size(), 2u);
    EXPECT_EQ(snapshot.diagnostics[0].id, "RenderSceneExtraction.InvalidMaterial");
    EXPECT_EQ(snapshot.diagnostics[1].id, "RenderSceneExtraction.InvalidMesh");
}

TEST(RenderSceneExtraction, LodOverrideAndLookupAreApplied)
{
    World::RenderWorld world;
    auto automatic = Entity(Vec3{0.8f, 0.0f, 0.0f});
    automatic.mesh = static_cast<World::MeshId>(2);
    (void)world.Create(automatic);

    auto overridden = Entity(Vec3{0.1f, 0.0f, 0.0f});
    overridden.mesh = static_cast<World::MeshId>(3);
    overridden.lodOverride = 3;
    (void)world.Create(overridden);

    Scene::RenderViewFamily family;
    family.AddView(View(Scene::RenderViewKind::Main, 0, 1));

    Scene::RenderSceneExtractionConfig config;
    config.lodLookup = [](World::MeshId) {
        LOD::LodThresholds thresholds;
        thresholds.byLevel = {0.0f, 0.2f, 0.6f, 0.9f};
        thresholds.count = 4;
        return thresholds;
    };

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(
        world,
        family,
        {},
        config);

    ASSERT_EQ(snapshot.drawPackets.size(), 2u);
    EXPECT_EQ(snapshot.drawPackets[0].lod, 2u);
    EXPECT_EQ(snapshot.drawPackets[1].lod, 3u);
}

TEST(RenderSceneExtraction, DrawPacketOrderingIsDeterministic)
{
    World::RenderWorld world;
    auto b = Entity(Vec3{0.0f, 0.0f, 0.0f});
    b.mesh = static_cast<World::MeshId>(2);
    b.material = static_cast<World::MaterialId>(20);
    (void)world.Create(b);

    auto a = Entity(Vec3{0.0f, 0.0f, 0.0f});
    a.mesh = static_cast<World::MeshId>(1);
    a.material = static_cast<World::MaterialId>(10);
    (void)world.Create(a);

    Scene::RenderViewFamily family;
    family.AddView(View(Scene::RenderViewKind::Debug, 2, 20));
    family.AddView(View(Scene::RenderViewKind::Main, 1, 10));

    const auto first = Scene::ExtractRenderFrameSnapshot(world, family, {});
    const auto second = Scene::ExtractRenderFrameSnapshot(world, family, {});

    ASSERT_EQ(first.drawPackets.size(), 4u);
    EXPECT_EQ(first.drawPackets[0].viewIndex, 1u);
    EXPECT_EQ(first.drawPackets[0].material, static_cast<World::MaterialId>(10));
    EXPECT_EQ(first.drawPackets[1].viewIndex, 1u);
    EXPECT_EQ(first.drawPackets[2].viewIndex, 2u);
    EXPECT_EQ(first.Serialize(), second.Serialize());
}

TEST(RenderSceneExtraction, PlaceholderViewProducesNoDrawPackets)
{
    World::RenderWorld world;
    (void)world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}));

    auto shadow = View(Scene::RenderViewKind::Shadow, 0, 1);
    shadow.placeholderOnly = true;

    Scene::RenderViewFamily family;
    family.AddView(shadow);
    family.AddView(View(Scene::RenderViewKind::Main, 1, 2));

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(world, family, {});

    EXPECT_EQ(snapshot.stats.viewCount, 2u);
    EXPECT_EQ(snapshot.stats.submittedViewCount, 2u);
    EXPECT_EQ(snapshot.stats.placeholderViewCount, 1u);
    EXPECT_EQ(snapshot.stats.packetCount, 1u);
    ASSERT_FALSE(snapshot.diagnostics.empty());
    EXPECT_EQ(snapshot.diagnostics.front().id,
              "RenderSceneExtraction.PlaceholderView");
}

TEST(RenderSceneExtraction, ResidencyDiagnosticsAttachToPackets)
{
    World::RenderWorld world;
    const auto entity = world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}));

    Scene::RenderViewFamily family;
    family.AddView(View(Scene::RenderViewKind::Main, 0, 1));

    Scene::RenderSceneExtractionConfig config;
    config.residencyDiagnostics = [entity](World::RenderEntityId id) {
        std::vector<Streaming::RenderStreamingDiagnostic> diagnostics;
        if (id == entity)
        {
            diagnostics.push_back({
                .id = "RenderGeometryResidency.LodFallback",
                .assetId = 10,
                .kind = Streaming::RenderStreamingAssetKind::Geometry,
                .detailLevel = 1,
                .message = "test fallback",
            });
        }
        return diagnostics;
    };

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(
        world,
        family,
        {},
        config);

    ASSERT_EQ(snapshot.drawPackets.size(), 1u);
    EXPECT_TRUE(snapshot.drawPackets[0].usesResidencyFallback);
    EXPECT_EQ(snapshot.stats.residencyFallbacks, 1u);
    ASSERT_FALSE(snapshot.diagnostics.empty());
    EXPECT_EQ(snapshot.diagnostics.front().id,
              "RenderSceneExtraction.ResidencyFallback");
}

TEST(RenderSceneExtraction, EmptyViewFamilyIsDiagnosticNoSubmit)
{
    World::RenderWorld world;
    (void)world.Create(Entity(Vec3{0.0f, 0.0f, 0.0f}));

    const auto snapshot = Scene::ExtractRenderFrameSnapshot(
        world,
        Scene::RenderViewFamily{},
        {});

    EXPECT_EQ(snapshot.stats.packetCount, 0u);
    ASSERT_EQ(snapshot.diagnostics.size(), 1u);
    EXPECT_EQ(snapshot.diagnostics.front().id,
              "RenderSceneExtraction.EmptyViewFamily");
}
