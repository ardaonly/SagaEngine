#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, NativeSamplerBindingProducesExpectedPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(0u, 0u, 255u));
    const auto sampler = CreateNativeSampler(*gfx, false, true);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Blue());
}

TEST_F(BindingGPU, MissingOptionalSamplerUsesCanonicalSampler)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(false, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoEmptyBindingSet(layout));
    ASSERT_TRUE(bindingSet.IsValid());

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, White());
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackSamplerUses, 1u);
}

TEST_F(BindingGPU, MissingRequiredSamplerRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, {}));
    EXPECT_FALSE(bindingSet.IsValid());
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, DestroyedSamplerRejectsDraw)
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

    gfx->DestroySampler(sampler);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
}

TEST_F(BindingGPU, StaleExplicitSamplerDoesNotUseFallback)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout =
        gfx->CreateBindingLayout(
            MakeNativeOptionalFallbackAlbedoBindingLayout(true, false));
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroySampler(sampler);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackSamplerUses, 0u);
    EXPECT_EQ(diagnostics.fallbackStaleExplicitResourceRejects, 1u);
}

