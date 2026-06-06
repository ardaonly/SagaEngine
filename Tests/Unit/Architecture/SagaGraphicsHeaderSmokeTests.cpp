/// @file SagaGraphicsHeaderSmokeTests.cpp
/// @brief Compile-smoke for the SagaGraphics public umbrella header.

#include "SagaEngine/Graphics/Graphics.h"

#include <gtest/gtest.h>

TEST(SagaGraphicsHeaderSmokeTests, GraphicsUmbrellaHeaderIsSelfContained)
{
    SagaEngine::Graphics::RenderBackendDesc backendDesc{};
    backendDesc.headless = true;

    SagaEngine::Graphics::SwapchainDesc swapchain{};
    swapchain.width = 64u;
    swapchain.height = 64u;

    SagaEngine::Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(backendDesc, swapchain));

    const auto status = backend.GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.selectedBackend, SagaEngine::Graphics::BackendPreference::Auto);
    EXPECT_EQ(status.health, SagaEngine::Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(status.failure, SagaEngine::Graphics::RenderBackendFailure::None);

    const auto capabilities = backend.GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        SagaEngine::Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(
        capabilities.compute,
        SagaEngine::Graphics::RenderFeatureSupport::Unsupported);
    EXPECT_EQ(
        SagaEngine::Graphics::ClampRenderQualityPreset(
            SagaEngine::Graphics::RenderQualityPreset::Ultra,
            capabilities),
        SagaEngine::Graphics::RenderQualityPreset::Low);

    const auto texture = backend.CreateTexture({});
    ASSERT_TRUE(texture.IsValid());
    backend.DestroyTexture(texture);

    const auto reusedTexture = backend.CreateTexture({});
    EXPECT_EQ(reusedTexture.index, texture.index);
    EXPECT_EQ(reusedTexture.generation, texture.generation + 1u);

    backend.Shutdown();
    EXPECT_FALSE(backend.CreateTexture({}).IsValid());
}
