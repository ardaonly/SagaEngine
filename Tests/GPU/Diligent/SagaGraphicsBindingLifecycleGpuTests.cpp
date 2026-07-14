#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, RejectedDrawDoesNotPoisonFollowingDraw)
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
    gfx->DestroyTexture(staleTexture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, staleSet), nullptr);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, validSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Green());
}

TEST_F(BindingGPU, FallbackBindingDoesNotLeakIntoExplicitBinding)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto explicitTexture =
        CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, true));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto fallbackSet =
        gfx->CreateBindingSet(MakeNativeAlbedoSamplerOnlyBindingSet(layout, sampler));
    const auto explicitSet =
        gfx->CreateBindingSet(
            MakeNativeAlbedoBindingSet(layout, explicitTexture, sampler));

    bool fallbackSubmitted = false;
    const auto fallbackCapture = DrawResolvedTextured(
        *gfx,
        pipeline,
        fallbackSet,
        &fallbackSubmitted);
    bool explicitSubmitted = false;
    const auto explicitCapture = DrawResolvedTextured(
        *gfx,
        pipeline,
        explicitSet,
        &explicitSubmitted);
    ASSERT_TRUE(fallbackSubmitted);
    ASSERT_TRUE(explicitSubmitted);
    ExpectCenterColor(fallbackCapture, White());
    ExpectCenterColor(explicitCapture, Red());
}

TEST_F(BindingGPU, BindingSurvivesMultipleFrames)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));

    for (int i = 0; i < 3; ++i)
    {
        bool submitted = false;
        const auto capture =
            DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
        ASSERT_TRUE(submitted);
        ExpectCenterColor(capture, Green());
    }
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_GE(diagnostics.nativeBindingCacheHits, 2u);
}

TEST_F(BindingGPU, DestroyedResourceIsRejectedOnFollowingFrame)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Red());

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
}

TEST_F(BindingGPU, FallbackResourcesRemainStableAcrossFrames)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    std::uint64_t generation = 0;
    for (int i = 0; i < 3; ++i)
    {
        bool submitted = false;
        const auto capture =
            DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
        ASSERT_TRUE(submitted);
        ExpectCenterColor(capture, White());
        const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
        if (i == 0)
        {
            generation = diagnostics.fallbackGeneration;
        }
        EXPECT_EQ(diagnostics.fallbackGeneration, generation);
    }
}

TEST_F(BindingGPU, ResizePreservesPersistentTextureBinding)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(0u, 255u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    m_Backend.OnResize(400u, 300u);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Green());
}

TEST_F(BindingGPU, ResizePreservesFallbackBinding)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    m_Backend.OnResize(400u, 300u);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, White());
}

TEST_F(BindingGPU, DestroyAfterSubmitDoesNotCrashUnderQuarantinePolicy)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Red());

    gfx->DestroyTexture(texture);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
}

TEST_F(BindingGPU, ReleaseClearsCacheAndQuarantine)
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
    ASSERT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 1u);
    gfx->Shutdown();
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
    EXPECT_EQ(gfx->GetNativeBindingQuarantinedSrbCountForTesting(), 0u);
}

TEST_F(BindingGPU, FailedFallbackInitializationDoesNotPublishEntry)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    gfx->ForceNextFallbackSamplerFailureForTesting(true);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, White());
}

TEST_F(BindingGPU, PerformanceSmokeRepeatedResolveKeepsHotPathCountersStable)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto warm = gfx->GetNativeBindingDiagnosticsForTesting();
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    }
    const auto hot = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 1u);
    EXPECT_EQ(hot.nativeBindingSrbCreates, warm.nativeBindingSrbCreates);
    EXPECT_EQ(hot.nativeBindingVariableLookups, warm.nativeBindingVariableLookups);
    EXPECT_EQ(
        hot.nativeBindingStaticVariableLookups,
        warm.nativeBindingStaticVariableLookups);
    EXPECT_EQ(hot.nativeBindingCacheHits, warm.nativeBindingCacheHits + 1000u);
}

