#include "SagaGraphicsGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(SagaGraphicsGPU, NativeVertexBufferCreates)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TriangleVertices();
    const auto buffer = CreateNativeVertexBuffer(*gfx, vertices);

    ASSERT_TRUE(buffer.IsValid());
    const auto query = gfx->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, SagaEngine::Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, SagaEngine::Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.logicalBytes, sizeof(vertices));
    EXPECT_NE(gfx->ResolveNativeBufferForTesting(buffer), nullptr);
}

TEST_F(SagaGraphicsGPU, NativeIndexBufferCreates)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto indices = TriangleIndices();
    const auto buffer = CreateNativeIndexBuffer(*gfx, indices);

    ASSERT_TRUE(buffer.IsValid());
    const auto query = gfx->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, SagaEngine::Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, SagaEngine::Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.logicalBytes, sizeof(indices));
    EXPECT_NE(gfx->ResolveNativeBufferForTesting(buffer), nullptr);
}

TEST_F(SagaGraphicsGPU, NativeIndexedMeshProducesPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TriangleVertices();
    const auto indices = TriangleIndices();
    const auto vertexBuffer = CreateNativeVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeIndexBuffer(*gfx, indices);
    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());

    bool submitted = false;
    const auto capture = DrawNativeIndexedTriangle(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        &submitted);

    ASSERT_TRUE(submitted);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 500u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 16u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              40u);
}

TEST_F(SagaGraphicsGPU, DestroyedHandleCannotDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TriangleVertices();
    const auto indices = TriangleIndices();
    const auto vertexBuffer = CreateNativeVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeIndexBuffer(*gfx, indices);
    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());

    gfx->DestroyBuffer(vertexBuffer);
    EXPECT_EQ(gfx->ResolveNativeBufferForTesting(vertexBuffer), nullptr);

    bool submitted = true;
    const auto capture = DrawNativeIndexedTriangle(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        &submitted);

    EXPECT_FALSE(submitted);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);
}

TEST_F(SagaGraphicsGPU, StaleGenerationCannotDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TriangleVertices();
    const auto indices = TriangleIndices();
    const auto staleVertexBuffer = CreateNativeVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeIndexBuffer(*gfx, indices);
    ASSERT_TRUE(staleVertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());

    gfx->DestroyBuffer(staleVertexBuffer);
    const auto replacementVertexBuffer = CreateNativeVertexBuffer(*gfx, vertices);
    ASSERT_TRUE(replacementVertexBuffer.IsValid());
    ASSERT_EQ(replacementVertexBuffer.index, staleVertexBuffer.index);
    ASSERT_NE(replacementVertexBuffer.generation, staleVertexBuffer.generation);
    EXPECT_EQ(gfx->ResolveNativeBufferForTesting(staleVertexBuffer), nullptr);
    EXPECT_NE(gfx->ResolveNativeBufferForTesting(replacementVertexBuffer), nullptr);

    bool submitted = true;
    const auto capture = DrawNativeIndexedTriangle(
        gfx->ResolveNativeBufferForTesting(staleVertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        &submitted);

    EXPECT_FALSE(submitted);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);
}

TEST_F(SagaGraphicsGPU, NativeTextureUploadProducesPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(*gfx);
    constexpr std::array<std::uint8_t, 16> kRedTexture{
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u};
    const auto texture = CreateNativeTexture(*gfx, kRedTexture);
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto pipeline = CreateNativeTexturedPipeline(*gfx);
    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    EXPECT_NE(gfx->ResolveNativeTextureSrvForTesting(texture), nullptr);
    EXPECT_NE(gfx->ResolveNativeSamplerForTesting(sampler), nullptr);
    EXPECT_NE(gfx->ResolveNativePipelineForTesting(pipeline), nullptr);

    bool submitted = false;
    const auto capture = DrawNativeTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        gfx->ResolveNativeTextureSrvForTesting(texture),
        gfx->ResolveNativeSamplerForTesting(sampler),
        &submitted);

    ASSERT_TRUE(submitted);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 24u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              100u);
}


