#include "BindingGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;

TEST_F(BindingGPU, SameKeyDifferentCanonicalLayoutRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());
    ASSERT_TRUE(gfx->CorruptNativeBindingSetCanonicalLayoutForTesting(bindingSet));

    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.canonicalLayoutMismatchRejects, 1u);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, StaleBindingLayoutRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    gfx->DestroyBindingLayout(layout);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleLayoutRejects, 1u);
}

TEST_F(BindingGPU, StaleBindingSetRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    gfx->DestroyBindingSet(bindingSet);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleBindingSetRejects, 1u);
}

TEST_F(BindingGPU, DestroyedPipelineRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipeline = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    gfx->DestroyPipeline(pipeline);
    EXPECT_EQ(gfx->ResolveNativeBindingSrbForTesting(pipeline, bindingSet), nullptr);
    const auto diagnostics = gfx->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.stalePipelineRejects, 1u);
}

TEST_F(BindingGPU, PipelineGenerationSeparatesCacheEntries)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto texture = CreateNativeTexture(*gfx, SolidTexture(255u, 0u, 0u));
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    const auto pipelineA = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto pipelineB = CreateNativeAlbedoPipeline(*gfx, layout);
    const auto bindingSet =
        gfx->CreateBindingSet(MakeNativeAlbedoBindingSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());

    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipelineA, bindingSet), nullptr);
    ASSERT_NE(gfx->ResolveNativeBindingSrbForTesting(pipelineB, bindingSet), nullptr);
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 2u);
}

TEST_F(BindingGPU, WrongResourceKindRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    const auto buffer =
        CreateNativeConstantBuffer(*gfx, {1.0f, 0.0f, 0.0f, 1.0f});
    const auto sampler = CreateNativeSampler(*gfx, true, false);
    const auto layout = gfx->CreateBindingLayout(MakeNativeAlbedoBindingLayout());
    SagaEngine::Graphics::GraphicsBindingSetDesc desc{};
    desc.layout = layout;
    SagaEngine::Graphics::GraphicsBindingResourceRef textureRef{};
    textureRef.slot = 0u;
    textureRef.stableId = 100u;
    textureRef.kind = SagaEngine::Graphics::GraphicsResourceKind::Buffer;
    textureRef.handle = buffer;
    desc.resources.push_back(textureRef);
    SagaEngine::Graphics::GraphicsBindingResourceRef samplerRef{};
    samplerRef.slot = 1u;
    samplerRef.stableId = 101u;
    samplerRef.kind = SagaEngine::Graphics::GraphicsResourceKind::Sampler;
    samplerRef.handle = sampler;
    desc.resources.push_back(samplerRef);

    const auto bindingSet = gfx->CreateBindingSet(desc);
    EXPECT_FALSE(bindingSet.IsValid());
    EXPECT_EQ(gfx->GetNativeBindingCacheEntryCountForTesting(), 0u);
}

TEST_F(BindingGPU, UnsupportedStorageBufferRejectsDraw)
{
    auto gfx = CreateNativeGraphicsBackend();
    auto layoutDesc = MakeNativeConstantBindingLayout();
    layoutDesc.slots[0].type =
        SagaEngine::Graphics::GraphicsBindingType::StorageBuffer;
    const auto layout = gfx->CreateBindingLayout(layoutDesc);
    EXPECT_FALSE(layout.IsValid());
}

