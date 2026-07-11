/// @file StarterArenaPlayableTests.cpp
/// @brief GPU-free contract tests for the visible StarterArena frame.

#include "StarterArenaPlayableScene.h"
#include "StarterArenaSimulation.h"

#include <gtest/gtest.h>

#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace
{

namespace App = SagaRuntimeApp;
namespace Backend = SagaEngine::Render::Backend;
namespace RenderWorld = SagaEngine::Render::World;

App::StarterArenaScene MakeScene()
{
    App::StarterArenaScene scene;
    scene.sceneId = "starter-arena-local-loop";
    scene.playerSpawn = {0.0, 0.0};
    scene.bounds = {-1.0, 1.0, -1.0, 1.0};
    scene.camera = {"fixed", 0.0, 0.0, 2.0, 2.0};
    scene.testInput = {4.0, 2.0};
    scene.expected = {1.0, 0.96, 15};
    return scene;
}

class TrackingBackend final : public Backend::IRenderBackend
{
public:
    bool Initialize(const Backend::SwapchainDesc&) override
    {
        initialized = true;
        return true;
    }
    void Shutdown() override { initialized = false; }
    void OnResize(std::uint32_t, std::uint32_t) override {}

    RenderWorld::MeshId CreateMesh(const SagaEngine::Render::MeshAsset&) override
    {
        if (failMeshCreation)
            return RenderWorld::MeshId::kInvalid;
        const auto id = static_cast<RenderWorld::MeshId>(nextMesh++);
        meshes.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    RenderWorld::MaterialId CreateMaterial(const SagaEngine::Render::MaterialRuntime&) override
    {
        if (failMaterialCreation)
            return RenderWorld::MaterialId::kInvalid;
        const auto id = static_cast<RenderWorld::MaterialId>(nextMaterial++);
        materials.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    void DestroyMesh(RenderWorld::MeshId id) override
    {
        meshes.erase(static_cast<std::uint32_t>(id));
    }
    void DestroyMaterial(RenderWorld::MaterialId id) override
    {
        materials.erase(static_cast<std::uint32_t>(id));
    }
    SagaEngine::Render::TextureHandle CreateTexture(std::uint32_t,
                                                    std::uint32_t,
                                                    const std::uint8_t*) override
    {
        if (failTextureCreation)
            return SagaEngine::Render::TextureHandle::kInvalid;
        const auto id = static_cast<SagaEngine::Render::TextureHandle>(nextTexture++);
        textures.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    void DestroyTexture(SagaEngine::Render::TextureHandle id) override
    {
        textures.erase(static_cast<std::uint32_t>(id));
    }
    void BeginFrame() override {}
    void Submit(const SagaEngine::Render::Scene::Camera&,
                const SagaEngine::Render::Scene::RenderView& view) override
    {
        submittedDraws = view.DrawCount();
    }
    void EndFrame() override {}

    bool failMeshCreation = false;
    bool failMaterialCreation = false;
    bool failTextureCreation = false;
    bool initialized = false;
    std::size_t submittedDraws = 0;
    std::unordered_set<std::uint32_t> meshes;
    std::unordered_set<std::uint32_t> materials;
    std::unordered_set<std::uint32_t> textures;

private:
    std::uint32_t nextMesh = 1;
    std::uint32_t nextMaterial = 1;
    std::uint32_t nextTexture = 1;
};

} // namespace

TEST(StarterArenaPlayableTests, SharedSimulationMatchesHeadlessExpectation)
{
    const auto scene = MakeScene();
    auto simulation = App::MakeStarterArenaSimulation(scene);
    for (std::uint32_t frame = 0; frame < 30u; ++frame)
    {
        App::StepStarterArenaSimulation(simulation, scene.testInput, 0.016);
    }
    EXPECT_EQ(simulation.ticks, 30u);
    EXPECT_EQ(simulation.clampCount, 15u);
    EXPECT_DOUBLE_EQ(simulation.position.x, 1.0);
    EXPECT_NEAR(simulation.position.y, 0.96, 0.000001);
}

TEST(StarterArenaPlayableTests, SceneBuildsCameraArenaAndPlayerDraw)
{
    TrackingBackend backend;
    RenderWorld::RenderWorld world;
    const auto sceneSource = MakeScene();
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, sceneSource, 1280u, 720u);

    ASSERT_TRUE(scene.IsValid());
    EXPECT_EQ(world.LiveCount(), 6u);
    EXPECT_FLOAT_EQ(scene.camera.position.x, 0.0f);
    EXPECT_FLOAT_EQ(scene.camera.position.y, 5.0f);
    EXPECT_FLOAT_EQ(scene.camera.position.z, 0.0f);

    auto view = App::BuildStarterArenaPlayableView(world, scene.camera);
    EXPECT_EQ(view.visited, 6u);
    EXPECT_GE(view.DrawCount(), 2u);
    EXPECT_TRUE(App::ViewContainsEntity(view, scene.player));

    backend.BeginFrame();
    backend.Submit(scene.camera, view);
    backend.EndFrame();
    EXPECT_EQ(backend.submittedDraws, view.DrawCount());

    App::DestroyStarterArenaPlayableScene(backend, world, scene);
    EXPECT_EQ(world.LiveCount(), 0u);
    EXPECT_TRUE(backend.meshes.empty());
    EXPECT_TRUE(backend.materials.empty());
    EXPECT_TRUE(backend.textures.empty());
}

TEST(StarterArenaPlayableTests, PlayerTransformUsesSimulationCoordinates)
{
    TrackingBackend backend;
    RenderWorld::RenderWorld world;
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, MakeScene(), 800u, 800u);
    ASSERT_TRUE(scene.IsValid());

    App::UpdateStarterArenaPlayerTransform(world, scene.player, {0.75, -0.5});
    const auto* player = world.Get(scene.player);
    ASSERT_NE(player, nullptr);
    EXPECT_FLOAT_EQ(player->transform.position.x, 0.75f);
    EXPECT_FLOAT_EQ(player->transform.position.y, 0.4f);
    EXPECT_FLOAT_EQ(player->transform.position.z, -0.5f);

    App::DestroyStarterArenaPlayableScene(backend, world, scene);
}

TEST(StarterArenaPlayableTests, PartialResourceFailureCleansAcceptedResources)
{
    TrackingBackend backend;
    backend.failMaterialCreation = true;
    RenderWorld::RenderWorld world;
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, MakeScene(), 1280u, 720u);

    EXPECT_FALSE(scene.IsValid());
    EXPECT_EQ(world.LiveCount(), 0u);
    EXPECT_TRUE(backend.meshes.empty());
    EXPECT_TRUE(backend.materials.empty());
    EXPECT_TRUE(backend.textures.empty());
}
