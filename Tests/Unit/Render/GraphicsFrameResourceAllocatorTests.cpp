/// @file GraphicsFrameResourceAllocatorTests.cpp
/// @brief Private CPU-side graphics frame resource allocator tests.

#include "SagaEngine/Graphics/Frame/FrameResourceAllocator.h"

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"

#include <gtest/gtest.h>

namespace
{

namespace Graphics = SagaEngine::Graphics;
namespace GraphicsPrivate = SagaEngine::Graphics::Private;

GraphicsPrivate::FrameResourceAllocatorConfig MakeConfig() noexcept
{
    GraphicsPrivate::FrameResourceAllocatorConfig config{};
    config.maxFramesInFlight = 2u;
    config.bytesPerFrame = 64u;
    config.defaultAlignment = 16u;
    return config;
}

Graphics::SwapchainDesc MakeSwapchain() noexcept
{
    Graphics::SwapchainDesc swapchain{};
    swapchain.width = 128u;
    swapchain.height = 128u;
    return swapchain;
}

} // namespace

TEST(GraphicsFrameResourceAllocator, AllocationsAreAligned)
{
    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));

    const auto first = allocator.Allocate(3u);
    const auto second = allocator.Allocate(4u, 8u);

    ASSERT_TRUE(first.IsValid());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(first.offsetBytes, 0u);
    EXPECT_EQ(first.alignmentBytes, 16u);
    EXPECT_EQ(second.offsetBytes, 8u);
    EXPECT_EQ(second.alignmentBytes, 8u);
}

TEST(GraphicsFrameResourceAllocator, OverflowIsRejected)
{
    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));

    EXPECT_TRUE(allocator.Allocate(64u).IsValid());
    EXPECT_FALSE(allocator.Allocate(1u).IsValid());

    const auto stats = allocator.GetStats();
    EXPECT_EQ(stats.currentFrameBytes, 64u);
    EXPECT_EQ(stats.failedAllocationCount, 1u);
}

TEST(GraphicsFrameResourceAllocator, PerFrameResetWorks)
{
    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));

    EXPECT_TRUE(allocator.Allocate(32u).IsValid());
    ASSERT_EQ(allocator.GetStats().currentFrameBytes, 32u);

    allocator.BeginFrame(2u);

    const auto allocation = allocator.Allocate(16u);
    ASSERT_TRUE(allocation.IsValid());
    EXPECT_EQ(allocation.frameIndex, 2u);
    EXPECT_EQ(allocation.frameSlot, 0u);
    EXPECT_EQ(allocation.offsetBytes, 0u);
    EXPECT_EQ(allocator.GetStats().currentFrameBytes, 16u);
}

TEST(GraphicsFrameResourceAllocator, FramesInFlightAreIsolated)
{
    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));

    const auto frame0 = allocator.Allocate(24u);
    ASSERT_TRUE(frame0.IsValid());
    EXPECT_EQ(frame0.frameSlot, 0u);

    allocator.BeginFrame(1u);
    const auto frame1 = allocator.Allocate(16u);
    ASSERT_TRUE(frame1.IsValid());
    EXPECT_EQ(frame1.frameSlot, 1u);
    EXPECT_EQ(frame1.offsetBytes, 0u);

    EXPECT_EQ(allocator.GetFrameBytesForTesting(0u), 24u);
    EXPECT_EQ(allocator.GetFrameBytesForTesting(1u), 16u);
}

TEST(GraphicsFrameResourceAllocator, ActiveFrameAllocationsDoNotOverwrite)
{
    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));

    const auto first = allocator.Allocate(16u);
    const auto second = allocator.Allocate(16u);

    ASSERT_TRUE(first.IsValid());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(first.frameSlot, second.frameSlot);
    EXPECT_NE(first.offsetBytes, second.offsetBytes);
    EXPECT_EQ(second.offsetBytes, 16u);
}

TEST(GraphicsFrameResourceAllocator, HeadlessAndFailedInitLifecycleRemainGreen)
{
    Graphics::NullGraphicsBackend backend;

    Graphics::RenderBackendDesc headlessDesc{};
    headlessDesc.headless = true;
    EXPECT_TRUE(backend.Initialize(headlessDesc, {}));
    EXPECT_EQ(
        backend.GetStatus().health,
        Graphics::RenderBackendHealth::Headless);

    GraphicsPrivate::FrameResourceAllocator allocator;
    ASSERT_TRUE(allocator.Initialize(MakeConfig()));
    EXPECT_TRUE(allocator.Allocate(16u).IsValid());

    backend.Shutdown();
    EXPECT_EQ(
        backend.GetStatus().health,
        Graphics::RenderBackendHealth::Shutdown);
}
