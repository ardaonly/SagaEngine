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
}
