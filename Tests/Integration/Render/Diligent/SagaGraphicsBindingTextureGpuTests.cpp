#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, NativeSampledTextureProducesExpectedPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Red());
}

TEST_F(BindingGPU, DifferentTextureBindingSetsProduceDifferentPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto red = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto green = CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto redSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, red, sampler));
    const auto greenSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, green, sampler));
    ASSERT_TRUE(redSet.IsValid());
    ASSERT_TRUE(greenSet.IsValid());

    bool redSubmitted = false;
    const auto redCapture = DrawResolvedTextured(
        *gfx,
        pipeline,
        redSet,
        &redSubmitted);
    bool greenSubmitted = false;
    const auto greenCapture = DrawResolvedTextured(
        *gfx,
        pipeline,
        greenSet,
        &greenSubmitted);

    ASSERT_TRUE(redSubmitted);
    ASSERT_TRUE(greenSubmitted);
    ExpectCenterColor(redCapture, Red());
    ExpectCenterColor(greenCapture, Green());
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 2u);
}

TEST_F(BindingGPU, BindingSetSwapChangesPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto red = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto blue = CreateNativeTexture(*gfx, SolidTexture(0u, 0u, 255u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto redSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, red, sampler));
    const auto blueSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, blue, sampler));
    ASSERT_TRUE(redSet.IsValid());
    ASSERT_TRUE(blueSet.IsValid());

    bool a = false;
    const auto first = DrawResolvedTextured(*gfx, pipeline, redSet, &a);
    bool b = false;
    const auto second = DrawResolvedTextured(*gfx, pipeline, blueSet, &b);
    bool c = false;
    const auto third = DrawResolvedTextured(*gfx, pipeline, redSet, &c);

    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_TRUE(c);
    ExpectCenterColor(first, Red());
    ExpectCenterColor(second, Blue());
    ExpectCenterColor(third, Red());
}

TEST_F(BindingGPU, CacheHitPreservesTexturePixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    bool firstSubmitted = false;
    const auto first = DrawResolvedTextured(
        *gfx,
        pipeline,
        bindingSet,
        &firstSubmitted);
    const auto afterMiss = gfx->GetNativeBindingDiagnosticsForTesting();
    bool secondSubmitted = false;
    const auto second = DrawResolvedTextured(
        *gfx,
        pipeline,
        bindingSet,
        &secondSubmitted);
    const auto afterHit = gfx->GetNativeBindingDiagnosticsForTesting();

    ASSERT_TRUE(firstSubmitted);
    ASSERT_TRUE(secondSubmitted);
    ExpectCenterColor(first, Green());
    ExpectCenterColor(second, Green());
    EXPECT_EQ(afterHit.nativeBindingSrbCreates, afterMiss.nativeBindingSrbCreates);
    EXPECT_EQ(
        afterHit.nativeBindingVariableLookups,
        afterMiss.nativeBindingVariableLookups);
    EXPECT_GT(afterHit.nativeBindingCacheHits, afterMiss.nativeBindingCacheHits);
}

TEST_F(BindingGPU, MissingOptionalTextureProducesWhitePixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, true));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoSamplerOnlyBindingSet(layout, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, White());
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackTextureUses, 1u);
}

TEST_F(BindingGPU, MissingRequiredTextureRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoSamplerOnlyBindingSet(layout, sampler));
    EXPECT_FALSE(bindingSet.IsValid());
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, DestroyedTextureRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
}

TEST_F(BindingGPU, StaleExplicitTextureDoesNotUseFallback)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, true));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackTextureUses, 0u);
    EXPECT_EQ(diagnostics.fallbackStaleExplicitResourceRejects, 1u);
}

TEST_F(BindingGPU, ValidTextureRecoversAfterRejectedDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto staleTexture =
        CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto validTexture =
        CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto staleSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, staleTexture, sampler));
    const auto validSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, validTexture, sampler));
    ASSERT_TRUE(staleSet.IsValid());
    ASSERT_TRUE(validSet.IsValid());
    gfx->DestroyTexture(staleTexture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, staleSet), nullptr);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, validSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Green());
}

