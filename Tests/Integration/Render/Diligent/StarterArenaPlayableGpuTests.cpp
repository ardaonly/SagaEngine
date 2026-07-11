/// @file StarterArenaPlayableGpuTests.cpp
/// @brief GPU pixel evidence for the visible StarterArena frame.

#include "DiligentGpuTestFixture.h"
#include "StarterArenaPlayableScene.h"
#include "StarterArenaInput.h"
#include "StarterArenaSimulation.h"

#include <SagaEngine/Render/World/RenderWorld.h>
#include <cstdint>
#include <filesystem>

namespace
{

SagaRuntimeApp::StarterArenaScene MakeStarterArenaGpuScene()
{
    SagaRuntimeApp::StarterArenaScene scene;
    scene.sceneId = "starter-arena-local-loop";
    scene.playerSpawn = {0.0, 0.0};
    scene.bounds = {-1.0, 1.0, -1.0, 1.0};
    scene.camera = {"fixed", 0.0, 0.0, 2.0, 2.0};
    scene.testInput = {4.0, 2.0};
    scene.expected = {1.0, 0.96, 15};
    return scene;
}

} // namespace

TEST_F(DiligentGPU, StarterArenaFrameProducesArenaPlayerAndBoundaryPixels)
{
    SagaEngine::Render::World::RenderWorld world;
    auto scene = SagaRuntimeApp::CreateStarterArenaPlayableScene(
        m_Backend.BackendInterface(), world, MakeStarterArenaGpuScene(), kWidth, kHeight);
    ASSERT_TRUE(scene.IsValid());

    std::string inputError;
    const auto inputScript = std::filesystem::path(SAGA_SOURCE_ROOT) /
                             "samples/StarterArena/Input/playable.synthetic-input.json";
    auto inputProvider = SagaRuntimeApp::CreateSyntheticInputProvider(inputScript, inputError);
    ASSERT_NE(inputProvider, nullptr) << inputError;
    auto simulation = SagaRuntimeApp::MakeStarterArenaSimulation(MakeStarterArenaGpuScene());
    for (std::uint32_t tick = 0; tick < 30u; ++tick)
    {
        SagaRuntimeApp::StepStarterArenaSimulation(
            simulation, inputProvider->Read(tick), 0.016);
    }
    SagaRuntimeApp::UpdateStarterArenaPlayerTransform(world, scene.player, simulation.position);
    EXPECT_NEAR(simulation.position.x, 0.48, 0.000001);
    EXPECT_NEAR(simulation.position.y, 0.48, 0.000001);
    const auto* player = world.Get(scene.player);
    ASSERT_NE(player, nullptr);
    EXPECT_NEAR(player->transform.position.x, 0.48f, 0.000001f);
    EXPECT_NEAR(player->transform.position.z, 0.48f, 0.000001f);

    const auto view = SagaRuntimeApp::BuildStarterArenaPlayableView(world, scene.camera);
    ASSERT_TRUE(SagaRuntimeApp::ViewContainsEntity(view, scene.player));
    ASSERT_GE(view.DrawCount(), 2u);

    const auto capture = RenderAndCapture(scene.camera, view);
    ASSERT_EQ(capture.width, kWidth);
    ASSERT_EQ(capture.height, kHeight);
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, {0u, 0u, 0u, 255u}, 8), 1000u);

    const auto playerPixels = SagaTests::Render::CountPixelsMatching(
        capture, [](SagaTests::Render::Rgba8 pixel) {
            return pixel.r > 180u && pixel.g > 120u && pixel.b < 120u;
        });
    const auto groundPixels = SagaTests::Render::CountPixelsMatching(
        capture, [](SagaTests::Render::Rgba8 pixel) {
            return pixel.g > pixel.r + 20u && pixel.g > pixel.b + 10u;
        });
    const auto boundaryPixels = SagaTests::Render::CountPixelsMatching(
        capture, [](SagaTests::Render::Rgba8 pixel) {
            const bool redBoundary = pixel.r > 40u &&
                                     static_cast<unsigned>(pixel.r) * 5u >
                                         static_cast<unsigned>(pixel.g) * 8u &&
                                     static_cast<unsigned>(pixel.r) * 5u >
                                         static_cast<unsigned>(pixel.b) * 8u;
            const bool bgraBoundary = pixel.b > 40u &&
                                      static_cast<unsigned>(pixel.b) * 5u >
                                          static_cast<unsigned>(pixel.g) * 8u &&
                                      static_cast<unsigned>(pixel.b) * 5u >
                                          static_cast<unsigned>(pixel.r) * 8u;
            return redBoundary || bgraBoundary;
        });
    EXPECT_GT(playerPixels, 20u);
    EXPECT_GT(groundPixels, 500u);
    EXPECT_GT(boundaryPixels, 20u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.submittedDrawItems, view.DrawCount());
    EXPECT_EQ(diagnostics.presentCalls, 1u);

    SagaRuntimeApp::DestroyStarterArenaPlayableScene(m_Backend.BackendInterface(), world, scene);
}
