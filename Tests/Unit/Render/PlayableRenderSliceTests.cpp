/// @file PlayableRenderSliceTests.cpp
/// @brief Verifies the smallest sandbox render scene reaches backend submit.

#include "SagaEngine/Render/RenderSubsystem.h"
#include "SagaSandbox/Render/PlayableRenderSlice.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{

namespace Math = SagaEngine::Math;
namespace Render = SagaEngine::Render;
namespace Backend = SagaEngine::Render::Backend;
namespace Scene = SagaEngine::Render::Scene;
namespace World = SagaEngine::Render::World;
namespace Slice = SagaSandbox::Render;

class PlayableSliceBackend final : public Backend::IRenderBackend
{
public:
    bool Initialize(const Backend::SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}

    World::MeshId CreateMesh(const Render::MeshAsset& asset) override
    {
        ++createMeshCalls;
        lastMeshVertexCount = asset.lods[0].vertexCountHint;
        lastMeshIndexCount = asset.lods[0].indexCountHint;
        return failMeshCreate ? World::MeshId::kInvalid
                              : static_cast<World::MeshId>(nextMesh++);
    }

    World::MaterialId CreateMaterial(
        const Render::MaterialRuntime& runtime) override
    {
        ++createMaterialCalls;
        lastMaterialAlbedo =
            runtime.textures[static_cast<std::size_t>(
                Render::MaterialTextureSlot::Albedo)];
        return failMaterialCreate ? World::MaterialId::kInvalid
                                  : static_cast<World::MaterialId>(
                                        nextMaterial++);
    }

    void DestroyMesh(World::MeshId) override { ++destroyMeshCalls; }
    void DestroyMaterial(World::MaterialId) override
    {
        ++destroyMaterialCalls;
    }

    Render::TextureHandle CreateTexture(
        std::uint32_t width,
        std::uint32_t height,
        const std::uint8_t* rgba) override
    {
        ++createTextureCalls;
        lastTextureWidth = width;
        lastTextureHeight = height;
        lastTextureHadData = rgba != nullptr;
        return failTextureCreate ? Render::TextureHandle::kInvalid
                                 : static_cast<Render::TextureHandle>(
                                       nextTexture++);
    }

    void DestroyTexture(Render::TextureHandle) override
    {
        ++destroyTextureCalls;
    }

    void BeginFrame() override
    {
        ++beginFrameCalls;
        lastSubmitDrawCounts.clear();
        lastSubmittedItems.clear();
    }

    void Submit(const Scene::Camera&, const Scene::RenderView& view) override
    {
        ++submitCalls;
        lastSubmitDrawCounts.push_back(view.DrawCount());
        lastSubmittedItems = view.drawItems;
    }

    void EndFrame() override { ++endFrameCalls; }

    bool failMeshCreate = false;
    bool failMaterialCreate = false;
    bool failTextureCreate = false;

    std::uint32_t nextMesh = 10u;
    std::uint32_t nextMaterial = 20u;
    std::uint32_t nextTexture = 30u;

    std::uint32_t createMeshCalls = 0;
    std::uint32_t createMaterialCalls = 0;
    std::uint32_t createTextureCalls = 0;
    std::uint32_t destroyMeshCalls = 0;
    std::uint32_t destroyMaterialCalls = 0;
    std::uint32_t destroyTextureCalls = 0;
    std::uint32_t beginFrameCalls = 0;
    std::uint32_t submitCalls = 0;
    std::uint32_t endFrameCalls = 0;

    std::uint32_t lastMeshVertexCount = 0;
    std::uint32_t lastMeshIndexCount = 0;
    std::uint32_t lastTextureWidth = 0;
    std::uint32_t lastTextureHeight = 0;
    bool lastTextureHadData = false;
    Render::TextureHandle lastMaterialAlbedo =
        Render::TextureHandle::kInvalid;
    std::vector<std::size_t> lastSubmitDrawCounts;
    std::vector<Scene::DrawItem> lastSubmittedItems;
};

} // namespace

TEST(PlayableRenderSlice, BuildsResourcesWorldCameraAndSubmitsOneDraw)
{
    World::RenderWorld world;
    PlayableSliceBackend backend;

    const auto scene = Slice::CreatePlayableRenderSliceScene(backend, world);
    ASSERT_TRUE(scene.IsValid());
    EXPECT_EQ(world.LiveCount(), 1u);

    EXPECT_EQ(backend.createMeshCalls, 1u);
    EXPECT_EQ(backend.createTextureCalls, 1u);
    EXPECT_EQ(backend.createMaterialCalls, 1u);
    EXPECT_EQ(backend.lastMeshVertexCount, 24u);
    EXPECT_EQ(backend.lastMeshIndexCount, 36u);
    EXPECT_EQ(backend.lastTextureWidth, 64u);
    EXPECT_EQ(backend.lastTextureHeight, 64u);
    EXPECT_TRUE(backend.lastTextureHadData);
    EXPECT_EQ(backend.lastMaterialAlbedo, scene.resources.texture);

    Render::RenderSubsystem subsystem(world, backend);
    (void)subsystem.AddCamera(Render::CameraRole::Main, scene.camera);
    subsystem.ExecuteFrame(Scene::FrameContext{});

    EXPECT_EQ(backend.beginFrameCalls, 1u);
    EXPECT_EQ(backend.submitCalls, 1u);
    EXPECT_EQ(backend.endFrameCalls, 1u);
    ASSERT_EQ(backend.lastSubmitDrawCounts.size(), 1u);
    EXPECT_EQ(backend.lastSubmitDrawCounts[0], 1u);
    EXPECT_EQ(subsystem.LastFrameStats().totalDrawn, 1u);

    ASSERT_EQ(backend.lastSubmittedItems.size(), 1u);
    EXPECT_EQ(backend.lastSubmittedItems[0].entity, scene.entity);
    EXPECT_EQ(backend.lastSubmittedItems[0].mesh, scene.resources.mesh);
    EXPECT_EQ(backend.lastSubmittedItems[0].material,
              scene.resources.material);
}

TEST(PlayableRenderSlice, HiddenEntityIsCulledBeforeBackendDraw)
{
    World::RenderWorld world;
    PlayableSliceBackend backend;
    const auto scene = Slice::CreatePlayableRenderSliceScene(backend, world);
    ASSERT_TRUE(scene.IsValid());
    ASSERT_TRUE(world.SetVisible(scene.entity, false));

    Render::RenderSubsystem subsystem(world, backend);
    (void)subsystem.AddCamera(Render::CameraRole::Main, scene.camera);
    subsystem.ExecuteFrame(Scene::FrameContext{});

    EXPECT_EQ(backend.submitCalls, 1u);
    ASSERT_EQ(backend.lastSubmitDrawCounts.size(), 1u);
    EXPECT_EQ(backend.lastSubmitDrawCounts[0], 0u);
    EXPECT_EQ(subsystem.LastFrameStats().totalVisited, 1u);
    EXPECT_EQ(subsystem.LastFrameStats().totalDrawn, 0u);
    EXPECT_EQ(subsystem.LastFrameStats().totalCulledHidden, 1u);
}

TEST(PlayableRenderSlice, InvalidMeshOrMaterialDoesNotReachBackendDraw)
{
    World::RenderWorld world;
    PlayableSliceBackend backend;

    World::RenderEntity missingMesh{};
    missingMesh.mesh = World::MeshId::kInvalid;
    missingMesh.material = static_cast<World::MaterialId>(22u);
    missingMesh.boundsRadius = 0.5f;
    const auto missingMeshId = world.Create(missingMesh);
    ASSERT_NE(missingMeshId, World::RenderEntityId::kInvalid);

    World::RenderEntity missingMaterial{};
    missingMaterial.mesh = static_cast<World::MeshId>(11u);
    missingMaterial.material = World::MaterialId::kInvalid;
    missingMaterial.boundsRadius = 0.5f;
    const auto missingMaterialId = world.Create(missingMaterial);
    ASSERT_NE(missingMaterialId, World::RenderEntityId::kInvalid);

    Render::RenderSubsystem subsystem(world, backend);
    (void)subsystem.AddCamera(
        Render::CameraRole::Main,
        Slice::MakePlayableMainCamera());
    subsystem.ExecuteFrame(Scene::FrameContext{});

    EXPECT_EQ(backend.submitCalls, 1u);
    ASSERT_EQ(backend.lastSubmitDrawCounts.size(), 1u);
    EXPECT_EQ(backend.lastSubmitDrawCounts[0], 0u);
    EXPECT_EQ(subsystem.LastFrameStats().totalVisited, 2u);
    EXPECT_EQ(subsystem.LastFrameStats().totalDrawn, 0u);
}

TEST(PlayableRenderSlice, EntityTransformReachesSubmittedModelMatrix)
{
    World::RenderWorld world;
    PlayableSliceBackend backend;
    const auto scene = Slice::CreatePlayableRenderSliceScene(backend, world);
    ASSERT_TRUE(scene.IsValid());

    Math::Transform transform{};
    transform.position = {0.25f, 0.5f, -0.25f};
    transform.scale = {2.0f, 3.0f, 4.0f};
    ASSERT_TRUE(world.SetTransform(scene.entity, transform));

    Render::RenderSubsystem subsystem(world, backend);
    (void)subsystem.AddCamera(Render::CameraRole::Main, scene.camera);
    subsystem.ExecuteFrame(Scene::FrameContext{});

    ASSERT_EQ(backend.lastSubmittedItems.size(), 1u);
    const auto& model = backend.lastSubmittedItems[0].model;
    EXPECT_FLOAT_EQ(model.At(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(model.At(1, 1), 3.0f);
    EXPECT_FLOAT_EQ(model.At(2, 2), 4.0f);
    EXPECT_FLOAT_EQ(model.At(0, 3), 0.25f);
    EXPECT_FLOAT_EQ(model.At(1, 3), 0.5f);
    EXPECT_FLOAT_EQ(model.At(2, 3), -0.25f);
}

TEST(PlayableRenderSlice, MaterialUploadFailureCleansAcceptedResources)
{
    PlayableSliceBackend backend;
    backend.failMaterialCreate = true;

    auto resources = Slice::CreatePlayableRenderSliceResources(backend);

    EXPECT_FALSE(resources.IsValid());
    EXPECT_EQ(resources.mesh, World::MeshId::kInvalid);
    EXPECT_EQ(resources.material, World::MaterialId::kInvalid);
    EXPECT_EQ(resources.texture, Render::TextureHandle::kInvalid);
    EXPECT_EQ(backend.createMeshCalls, 1u);
    EXPECT_EQ(backend.createTextureCalls, 1u);
    EXPECT_EQ(backend.createMaterialCalls, 1u);
    EXPECT_EQ(backend.destroyMeshCalls, 1u);
    EXPECT_EQ(backend.destroyTextureCalls, 1u);
    EXPECT_EQ(backend.destroyMaterialCalls, 0u);
}
