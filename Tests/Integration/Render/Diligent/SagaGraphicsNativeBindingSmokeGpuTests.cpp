#include "SagaGraphicsGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(SagaGraphicsGPU, NativeBindingCacheCreatesAndReusesSrb)
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
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet = gfx->CreateBindingSet(
        MakeNativeAlbedoBindingSet(layout, texture, sampler));

    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(bindingSet.IsValid());
    EXPECT_NE(gfx->ResolveNativeTextureSrvForTesting(texture), nullptr);
    EXPECT_NE(gfx->ResolveNativeSamplerForTesting(sampler), nullptr);
    EXPECT_NE(gfx->ResolveNativePipelineForTesting(pipeline), nullptr);

    auto* firstSrb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(firstSrb, nullptr);
    const auto afterMiss = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(afterMiss.nativeBindingResolveSuccesses, 1u);
    EXPECT_EQ(afterMiss.nativeBindingCacheMisses, 1u);
    EXPECT_EQ(afterMiss.nativeBindingCacheHits, 0u);
    EXPECT_EQ(afterMiss.nativeBindingSrbCreates, 1u);
    EXPECT_EQ(afterMiss.nativeBindingVariableLookups, 2u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);

    auto* secondSrb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(secondSrb, nullptr);
    EXPECT_EQ(secondSrb, firstSrb);
    const auto afterHit = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(afterHit.nativeBindingResolveSuccesses, 2u);
    EXPECT_EQ(afterHit.nativeBindingCacheMisses, 1u);
    EXPECT_EQ(afterHit.nativeBindingCacheHits, 1u);
    EXPECT_EQ(afterHit.nativeBindingSrbCreates, 1u);
    EXPECT_EQ(afterHit.nativeBindingVariableLookups, 2u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);

    bool submitted = false;
    const auto capture = DrawNativeBoundTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        secondSrb,
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

TEST_F(SagaGraphicsGPU, NativeBindingCacheInvalidatesDestroyedTexture)
{
    auto gfx = CreateNativeGraphicsBackend();
    constexpr std::array<std::uint8_t, 16> kRedTexture{
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u};
    const auto texture = CreateNativeTexture(*gfx, kRedTexture);
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet = gfx->CreateBindingSet(
        MakeNativeAlbedoBindingSet(layout, texture, sampler));

    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(bindingSet.IsValid());

    auto* firstSrb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(firstSrb, nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 0u);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
    EXPECT_EQ(gfx->ResolveNativeTextureSrvForTesting(texture), nullptr);

    EXPECT_EQ(
        gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet),
        nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.nativeBindingSrbCreates, 1u);
    EXPECT_EQ(diagnostics.nativeBindingCacheInvalidations, 1u);
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
    EXPECT_EQ(diagnostics.nativeBindingCacheHits, 0u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
}

TEST_F(SagaGraphicsGPU, NativeBindingMissingOptionalTextureUsesWhiteFallback)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(*gfx);
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, true));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoSamplerOnlyBindingSet(layout, sampler));

    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(bindingSet.IsValid());

    auto* srb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(srb, nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_TRUE(diagnostics.fallbackLiveState);
    EXPECT_EQ(diagnostics.fallbackTextureUses, 1u);
    EXPECT_EQ(diagnostics.fallbackSamplerUses, 0u);
    EXPECT_EQ(diagnostics.fallbackTextureCreates, 1u);
    EXPECT_EQ(diagnostics.fallbackSamplerCreates, 1u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);

    bool submitted = false;
    const auto capture = DrawNativeBoundTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        srb,
        &submitted);

    ASSERT_TRUE(submitted);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 24u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::ColorNear(
                          c,
                          {255u, 255u, 255u, 255u},
                          12);
                  }),
              100u);
}

TEST_F(SagaGraphicsGPU, NativeBindingMissingRequiredTextureRejects)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(layout.IsValid());

    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoSamplerOnlyBindingSet(layout, sampler));
    EXPECT_FALSE(bindingSet.IsValid());
    EXPECT_EQ(
        gfx->GetLastResourceFailure(),
        SagaEngine::Graphics::GraphicsResourceFailure::InvalidBindingSetDesc);
}

TEST_F(SagaGraphicsGPU, NativeBindingStaleExplicitTextureDoesNotFallback)
{
    auto gfx = CreateNativeGraphicsBackend();
    constexpr std::array<std::uint8_t, 16> kRedTexture{
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u,
        255u, 0u, 0u, 255u};
    const auto texture = CreateNativeTexture(*gfx, kRedTexture);
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, true));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet = gfx->CreateBindingSet(
        MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(bindingSet.IsValid());

    auto* firstSrb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(firstSrb, nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackTextureUses, 0u);
    EXPECT_EQ(diagnostics.fallbackStaleExplicitResourceRejects, 1u);
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(SagaGraphicsGPU, NativeBindingMissingOptionalSamplerUsesCanonicalSampler)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(*gfx);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));

    ASSERT_TRUE(vertexBuffer.IsValid());
    ASSERT_TRUE(indexBuffer.IsValid());
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(bindingSet.IsValid());

    auto* srb = gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet);
    ASSERT_NE(srb, nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackTextureUses, 1u);
    EXPECT_EQ(diagnostics.fallbackSamplerUses, 1u);
    EXPECT_EQ(diagnostics.fallbackTextureCreates, 1u);
    EXPECT_EQ(diagnostics.fallbackSamplerCreates, 1u);

    bool submitted = false;
    const auto capture = DrawNativeBoundTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        srb,
        &submitted);

    ASSERT_TRUE(submitted);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 24u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::ColorNear(
                          c,
                          {255u, 255u, 255u, 255u},
                          12);
                  }),
              100u);
}

TEST_F(SagaGraphicsGPU, NativeShaderPipelineProducesPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(*gfx);
    constexpr std::array<std::uint8_t, 16> kGreenTexture{
        0u, 255u, 0u, 255u,
        0u, 255u, 0u, 255u,
        0u, 255u, 0u, 255u,
        0u, 255u, 0u, 255u};
    const auto texture = CreateNativeTexture(*gfx, kGreenTexture);
    const auto sampler = CreateNativeSampler(*gfx, false, false);
    const auto pipeline = CreateNativeTexturedPipeline(*gfx);
    ASSERT_TRUE(pipeline.IsValid());

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
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              100u);
}

TEST_F(SagaGraphicsGPU, LinearAndSrgbFormatsCreateAndSample)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto vertices = TexturedQuadVertices();
    const auto vertexBuffer = CreateNativeTexturedVertexBuffer(*gfx, vertices);
    const auto indexBuffer = CreateNativeQuadIndexBuffer(*gfx);
    constexpr std::array<std::uint8_t, 16> kGrayTexture{
        128u, 128u, 128u, 255u,
        128u, 128u, 128u, 255u,
        128u, 128u, 128u, 255u,
        128u, 128u, 128u, 255u};
    const auto linearTexture = CreateNativeTexture(
        *gfx,
        kGrayTexture,
        SagaEngine::Graphics::ResourceFormat::Rgba8Unorm);
    const auto srgbTexture = CreateNativeTexture(
        *gfx,
        kGrayTexture,
        SagaEngine::Graphics::ResourceFormat::Rgba8UnormSrgb);
    const auto sampler = CreateNativeSampler(*gfx, false, false);
    const auto pipeline = CreateNativeTexturedPipeline(*gfx);
    ASSERT_TRUE(linearTexture.IsValid());
    ASSERT_TRUE(srgbTexture.IsValid());

    bool submitted = false;
    const auto linearCapture = DrawNativeTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        gfx->ResolveNativeTextureSrvForTesting(linearTexture),
        gfx->ResolveNativeSamplerForTesting(sampler),
        &submitted);
    ASSERT_TRUE(submitted);
    const auto srgbCapture = DrawNativeTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        gfx->ResolveNativeTextureSrvForTesting(srgbTexture),
        gfx->ResolveNativeSamplerForTesting(sampler),
        &submitted);
    ASSERT_TRUE(submitted);

    const auto linearLuma = SagaTests::Render::AverageRegionLuminance(
        linearCapture,
        linearCapture.width / 2u,
        linearCapture.height / 2u,
        24u);
    const auto srgbLuma = SagaTests::Render::AverageRegionLuminance(
        srgbCapture,
        srgbCapture.width / 2u,
        srgbCapture.height / 2u,
        24u);
    EXPECT_GT(linearLuma, 20.0f);
    EXPECT_GT(srgbLuma, 20.0f);
    EXPECT_NE(gfx->GetTextureNativeSerialForTesting(linearTexture),
              gfx->GetTextureNativeSerialForTesting(srgbTexture));
}

TEST_F(SagaGraphicsGPU, DestroyedTextureCannotBind)
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
    ASSERT_TRUE(texture.IsValid());
    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeTextureSrvForTesting(texture), nullptr);

    bool submitted = true;
    const auto capture = DrawNativeTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        gfx->ResolveNativeTextureSrvForTesting(texture),
        gfx->ResolveNativeSamplerForTesting(sampler),
        &submitted);

    EXPECT_FALSE(submitted);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);
}

TEST_F(SagaGraphicsGPU, DestroyedSamplerCannotBind)
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
    ASSERT_TRUE(sampler.IsValid());
    gfx->DestroySampler(sampler);
    EXPECT_EQ(gfx->ResolveNativeSamplerForTesting(sampler), nullptr);

    bool submitted = true;
    const auto capture = DrawNativeTexturedQuad(
        gfx->ResolveNativeBufferForTesting(vertexBuffer),
        gfx->ResolveNativeBufferForTesting(indexBuffer),
        gfx->ResolveNativePipelineForTesting(pipeline),
        gfx->ResolveNativeTextureSrvForTesting(texture),
        gfx->ResolveNativeSamplerForTesting(sampler),
        &submitted);

    EXPECT_FALSE(submitted);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);
}

