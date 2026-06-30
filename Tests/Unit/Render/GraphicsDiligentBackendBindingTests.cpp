/// @file GraphicsDiligentBackendBindingTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, LogicalBindingLayoutAndSetLifecycleMatchContract)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto layout = backend->CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto layoutQuery = backend->QueryBindingLayout(layout);
    EXPECT_TRUE(layoutQuery.live);
    EXPECT_NE(layoutQuery.compatibilityKey, 0u);
    EXPECT_EQ(layoutQuery.bindingCount, 2u);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto sampler = backend->CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    Graphics::GraphicsBindingSetDesc set =
        MakeTextureSet(layout, texture, sampler);
    const auto bindingSet = backend->CreateBindingSet(set);
    ASSERT_TRUE(bindingSet.IsValid());
    const auto setQuery = backend->QueryBindingSet(bindingSet);
    EXPECT_TRUE(setQuery.live);
    EXPECT_EQ(setQuery.layout.index, layout.index);
    EXPECT_EQ(setQuery.compatibilityKey, layoutQuery.compatibilityKey);
    EXPECT_EQ(setQuery.bindingCount, 2u);

    backend->DestroyBindingSet(bindingSet);
    EXPECT_FALSE(backend->QueryBindingSet(bindingSet).live);
    backend->DestroyBindingSet(bindingSet);

    backend->DestroyBindingLayout(layout);
    EXPECT_FALSE(backend->QueryBindingLayout(layout).live);
}

TEST(GraphicsDiligentBackendAdapter, LogicalBindingSetRejectsAlreadyStaleResource)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto layout = backend->CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto sampler = backend->CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    backend->DestroyTexture(texture);

    EXPECT_FALSE(
        backend->CreateBindingSet(MakeTextureSet(layout, texture, sampler))
            .IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBindingSetDesc);
}

TEST(GraphicsDiligentBackendAdapter, LogicalBindingPipelineCompatibilityIsChecked)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto layout = backend->CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto key = backend->QueryBindingLayout(layout).compatibilityKey;

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingLayout = layout;
    pipeline.bindingCompatibilityKey = key;
    EXPECT_TRUE(backend->CreatePipeline(pipeline).IsValid());

    pipeline.bindingCompatibilityKey = key + 1u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());

    backend->DestroyBindingLayout(layout);
    pipeline.bindingCompatibilityKey = 0u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
}

TEST(
    GraphicsDiligentBackendAdapter,
    NativePreparedHandlesValidateBindingsWithoutUpload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    BindFakeNativeDeviceServices(*backend);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto sampler = backend->CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));
    bindingSet.resources.push_back(MakeSamplerBinding(sampler));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryDiligentBindingResource,
        backend.get());
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.code, Graphics::GraphicsBindingValidationCode::None);
    EXPECT_EQ(
        backend->QueryTextureForTesting(texture).backing,
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(
        backend->QuerySamplerForTesting(sampler).backing,
        Graphics::GraphicsResourceBacking::NativeGpuFuture);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}
