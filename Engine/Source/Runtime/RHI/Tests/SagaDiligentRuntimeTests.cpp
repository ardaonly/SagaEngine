/// @file DiligentGraphicsRuntimeTests.cpp
/// @brief Private Diligent runtime frame, upload, and command tests.

#include "SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.h"

#include <gtest/gtest.h>

namespace
{

namespace Runtime = SagaEngine::Graphics::Backends::Diligent::Runtime;
namespace Graphics = SagaEngine::Graphics;

Graphics::TextureHandle Texture(std::uint32_t index = 1u) noexcept
{
    Graphics::TextureHandle handle{};
    handle.index = index;
    handle.generation = 1u;
    return handle;
}

Graphics::BufferHandle Buffer(std::uint32_t index = 1u) noexcept
{
    Graphics::BufferHandle handle{};
    handle.index = index;
    handle.generation = 1u;
    return handle;
}

Graphics::PipelineHandle Pipeline(std::uint32_t index = 1u) noexcept
{
    Graphics::PipelineHandle handle{};
    handle.index = index;
    handle.generation = 1u;
    return handle;
}

Graphics::BindingSetHandle BindingSet(std::uint32_t index = 1u) noexcept
{
    Graphics::BindingSetHandle handle{};
    handle.index = index;
    handle.generation = 1u;
    return handle;
}

} // namespace

TEST(DiligentGraphicsRuntime, BeginFrameRequiresInitializedNativeRuntime)
{
    Runtime::DiligentGraphicsRuntime runtime(2u);

    auto frame0 = runtime.BeginFrame(0u);

    EXPECT_FALSE(frame0.token.IsValid());
    EXPECT_EQ(frame0.token.frameIndex, 0u);
    EXPECT_EQ(frame0.token.submissionSerial, 0u);
}

TEST(DiligentGraphicsRuntime, UploadArenaAlignsSlicesAndAllocatesSecondaryChunks)
{
    Runtime::DiligentFrameUploadArena arena(1u, 64u, 64u, 16u);
    Runtime::FrameToken token{};
    token.frameSlot = 0u;
    token.frameSlotGeneration = 1u;
    token.submissionSerial = 1u;
    arena.BeginFrame(token);

    const auto first = arena.AllocateUniform(12u);
    const auto second = arena.AllocateUniform(12u);
    const auto overflow = arena.AllocateUniform(80u);

    ASSERT_TRUE(first.IsValid());
    ASSERT_TRUE(second.IsValid());
    ASSERT_TRUE(overflow.IsValid());
    EXPECT_EQ(first.byteOffset % 16u, 0u);
    EXPECT_EQ(second.byteOffset % 16u, 0u);
    EXPECT_EQ(overflow.byteOffset % 16u, 0u);
    EXPECT_EQ(overflow.chunkIndex, 1u);

    const auto diagnostics = arena.Diagnostics();
    EXPECT_EQ(diagnostics.uniformOverflowCount, 1u);
    EXPECT_EQ(diagnostics.secondaryChunkCount, 1u);
    EXPECT_EQ(diagnostics.largestUniformAllocation, 80u);
}

TEST(DiligentGraphicsRuntime, CommandEncoderRejectsInvalidFrameAndPassOrdering)
{
    Runtime::GraphicsCommandEncoder invalid({});
    EXPECT_FALSE(invalid.Draw(3u));
    EXPECT_EQ(
        invalid.Diagnostics().lastError,
        Runtime::GraphicsCommandError::FrameNotOpen);

    Runtime::FrameToken token{};
    token.submissionSerial = 1u;
    Runtime::GraphicsCommandEncoder encoder(token);

    EXPECT_FALSE(encoder.Draw(3u));
    EXPECT_EQ(
        encoder.Diagnostics().lastError,
        Runtime::GraphicsCommandError::RenderPassNotOpen);

    auto pass = encoder.BeginRenderPass();
    EXPECT_TRUE(pass.SetPipeline(Pipeline()));
    EXPECT_TRUE(pass.SetBindingSet(BindingSet()));
    EXPECT_TRUE(pass.SetVertexBuffer(Buffer()));
    EXPECT_TRUE(pass.SetIndexBuffer(Buffer(2u)));
    EXPECT_TRUE(pass.Draw(3u));
    EXPECT_TRUE(pass.DrawIndexed(6u));
    EXPECT_TRUE(pass.EndRenderPass());
    EXPECT_TRUE(encoder.Submit());
    EXPECT_TRUE(encoder.Present());

    const auto diagnostics = encoder.Diagnostics();
    EXPECT_EQ(diagnostics.passCount, 1u);
    EXPECT_EQ(diagnostics.drawCount, 2u);
    EXPECT_EQ(diagnostics.indexedDrawCount, 1u);
    EXPECT_EQ(diagnostics.submitCount, 1u);
    EXPECT_EQ(diagnostics.presentCount, 1u);
}

TEST(DiligentGraphicsRuntime, SubmitRejectsOpenRenderPass)
{
    Runtime::FrameToken token{};
    token.submissionSerial = 1u;
    Runtime::GraphicsCommandEncoder encoder(token);

    (void)encoder.BeginRenderPass();
    EXPECT_FALSE(encoder.Submit());
    EXPECT_EQ(
        encoder.Diagnostics().lastError,
        Runtime::GraphicsCommandError::RenderPassAlreadyOpen);
}
