/// @file StarterArenaPlayableGpuTests.cpp
/// @brief GPU pixel evidence for the visible StarterArena frame.

#include "DiligentGpuTestFixture.h"
#include "StarterArenaPlayableScene.h"

#include <SagaEngine/Render/World/RenderWorld.h>

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
