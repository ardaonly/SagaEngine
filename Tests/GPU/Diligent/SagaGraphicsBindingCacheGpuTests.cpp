#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, SrbCreatedOnceAfterWarmup)
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
    const auto warm = gfx->GetNativeBindingDiagnosticsForTesting();

    for (int i = 0; i < 25; ++i)
    {
        ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    }
    const auto hot = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(hot.nativeBindingSrbCreates, warm.nativeBindingSrbCreates);
    EXPECT_EQ(hot.nativeBindingVariableLookups, warm.nativeBindingVariableLookups);
    EXPECT_EQ(
        hot.nativeBindingStaticVariableLookups,
        warm.nativeBindingStaticVariableLookups);
    EXPECT_EQ(hot.nativeBindingCacheHits, warm.nativeBindingCacheHits + 25u);
}

TEST_F(BindingGPU, VariableLookupOccursOnlyOnCacheMiss)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto miss = gfx->GetNativeBindingDiagnosticsForTesting();
    ASSERT_GT(miss.nativeBindingVariableLookups, 0u);

    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto hit = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(hit.nativeBindingVariableLookups, miss.nativeBindingVariableLookups);
    EXPECT_EQ(hit.nativeBindingCacheHits, miss.nativeBindingCacheHits + 1u);
}

TEST_F(BindingGPU, GetStaticVariableLookupOccursOnlyOnCacheMiss)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto miss = gfx->GetNativeBindingDiagnosticsForTesting();

    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto hit = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(
        hit.nativeBindingStaticVariableLookups,
        miss.nativeBindingStaticVariableLookups);
    EXPECT_EQ(hit.nativeBindingCacheHits, miss.nativeBindingCacheHits + 1u);
}

TEST_F(BindingGPU, CacheHitDoesNotCreateFallbackResources)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto warm = gfx->GetNativeBindingDiagnosticsForTesting();
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto hot = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(hot.fallbackTextureCreates, warm.fallbackTextureCreates);
    EXPECT_EQ(hot.fallbackSamplerCreates, warm.fallbackSamplerCreates);
    EXPECT_EQ(hot.nativeBindingSrbCreates, warm.nativeBindingSrbCreates);
}

TEST_F(BindingGPU, BindingSetChangeCreatesSeparateEntry)
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
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, redSet), nullptr);
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, greenSet), nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 2u);
}

TEST_F(BindingGPU, PipelineChangeCreatesSeparateEntry)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto firstPipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto secondPipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(
        gfx->ResolveNativeBindingSrbForTesting(firstPipeline, bindingSet),
        nullptr);
    ASSERT_NE(
        gfx->ResolveNativeBindingSrbForTesting(secondPipeline, bindingSet),
        nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 2u);
}

TEST_F(BindingGPU, ResourceGenerationChangeInvalidatesEntry)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
}

TEST_F(BindingGPU, InvalidatedSrbIsRemovedFromLookup)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    ASSERT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
}

TEST_F(BindingGPU, InvalidatedSrbMovesToQuarantine)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroyTexture(texture);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.quarantinedSrbCount, 1u);
    EXPECT_EQ(diagnostics.quarantinedSrbPeak, 1u);
}

TEST_F(BindingGPU, QuarantinedEntryIsNeverReused)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto staleTexture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto validTexture = CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto staleSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, staleTexture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, staleSet), nullptr);
    gfx->DestroyTexture(staleTexture);
    ASSERT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);

    const auto validSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, validTexture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, validSet), nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
}

TEST_F(BindingGPU, FallbackGenerationChangeInvalidatesEntry)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto warm = gfx->GetNativeBindingDiagnosticsForTesting();
    gfx->ReleaseFallbackResourcesForTesting();
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto afterRecreate = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_GT(afterRecreate.fallbackGeneration, warm.fallbackGeneration);
    EXPECT_GT(afterRecreate.nativeBindingCacheMisses, warm.nativeBindingCacheMisses);
}

TEST_F(BindingGPU, FailedResolveDoesNotPublishEntry)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, MultipleBindingSetsRemainIndependent)
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

    bool redSubmitted = false;
    const auto redCapture = DrawResolvedTextured(*gfx, pipeline, redSet, &redSubmitted);
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
}

