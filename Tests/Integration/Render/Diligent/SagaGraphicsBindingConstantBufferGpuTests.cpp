#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, ConstantBufferChangesPixelColor)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer =
        CreateNativeConstantBuffer(*gfx, {0.0f, 1.0f, 0.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, buffer));
    ASSERT_TRUE(bindingSet.IsValid());

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, bindingSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Green());
}

TEST_F(BindingGPU, DifferentConstantBuffersProduceDifferentPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto redBuffer =
        CreateNativeConstantBuffer(*gfx, {1.0f, 0.0f, 0.0f, 1.0f});
    const auto blueBuffer =
        CreateNativeConstantBuffer(*gfx, {0.0f, 0.0f, 1.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto redSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, redBuffer));
    const auto blueSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, blueBuffer));
    ASSERT_TRUE(redSet.IsValid());
    ASSERT_TRUE(blueSet.IsValid());

    bool redSubmitted = false;
    const auto redCapture = DrawResolvedTextured(*gfx, pipeline, redSet, &redSubmitted);
    bool blueSubmitted = false;
    const auto blueCapture = DrawResolvedTextured(
        *gfx,
        pipeline,
        blueSet,
        &blueSubmitted);

    ASSERT_TRUE(redSubmitted);
    ASSERT_TRUE(blueSubmitted);
    ExpectCenterColor(redCapture, Red());
    ExpectCenterColor(blueCapture, Blue());
}

TEST_F(BindingGPU, ConstantBufferCacheHitPreservesPixels)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer =
        CreateNativeConstantBuffer(*gfx, {1.0f, 0.0f, 0.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, buffer));
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
    ExpectCenterColor(first, Red());
    ExpectCenterColor(second, Red());
    EXPECT_EQ(afterHit.nativeBindingSrbCreates, afterMiss.nativeBindingSrbCreates);
    EXPECT_EQ(
        afterHit.nativeBindingVariableLookups,
        afterMiss.nativeBindingVariableLookups);
}

TEST_F(BindingGPU, DestroyedConstantBufferRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer =
        CreateNativeConstantBuffer(*gfx, {1.0f, 0.0f, 0.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, buffer));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);

    gfx->DestroyBuffer(buffer);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
}

TEST_F(BindingGPU, WrongBufferUsageRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer = CreateNativeWrongUsageBuffer(*gfx);
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, buffer));
    ASSERT_TRUE(bindingSet.IsValid());

    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleResourceRejects, 1u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, NonZeroSubrangeRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer =
        CreateNativeConstantBuffer(*gfx, {1.0f, 0.0f, 0.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, buffer, 4u, 8u));
    ASSERT_TRUE(bindingSet.IsValid());

    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.unsupportedBindingRejects, 1u);
}

TEST_F(BindingGPU, ValidConstantBufferRecoversAfterRejectedDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto badBuffer = CreateNativeWrongUsageBuffer(*gfx);
    const auto goodBuffer =
        CreateNativeConstantBuffer(*gfx, {0.0f, 1.0f, 0.0f, 1.0f});
    const auto layout = gfx->CreateBindingLayout(MakeNativeConstantBindingLayout());
    const auto pipeline = CreateNativeConstantPipeline(*gfx, layout);
    const auto badSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, badBuffer));
    const auto goodSet =
        gfx->CreateBindingSet(MakeNativeConstantBindingSet(layout, goodBuffer));
    ASSERT_TRUE(badSet.IsValid());
    ASSERT_TRUE(goodSet.IsValid());
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, badSet), nullptr);

    bool submitted = false;
    const auto capture = DrawResolvedTextured(*gfx, pipeline, goodSet, &submitted);
    ASSERT_TRUE(submitted);
    ExpectCenterColor(capture, Green());
}

