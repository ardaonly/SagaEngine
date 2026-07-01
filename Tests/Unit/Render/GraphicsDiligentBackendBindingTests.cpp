/// @file GraphicsDiligentBackendBindingTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include "SagaEngine/Graphics/Backends/Diligent/DiligentBindingCompiler.h"

#include <gtest/gtest.h>

namespace
{

Graphics::GraphicsBindingLayoutDesc MakeOptionalFallbackTextureLayout()
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    Graphics::GraphicsBindingLayoutSlot texture{};
    texture.slot = 0u;
    texture.stableId = 200u;
    texture.type = Graphics::GraphicsBindingType::SampledTexture;
    texture.stages = Graphics::kGraphicsShaderStageFragment;
    texture.required = false;
    texture.fallbackPolicy =
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampledTexture;
    layout.slots.push_back(texture);
    return layout;
}

Graphics::GraphicsBindingLayoutDesc MakeStorageBufferLayout()
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots.resize(1u);
    layout.slots[0].type = Graphics::GraphicsBindingType::StorageBuffer;
    return layout;
}

} // namespace

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
    EXPECT_EQ(backend->GetCompiledBindingLayoutCountForTesting(), 1u);
    const auto* compiledLayout =
        backend->ResolveCompiledBindingLayoutForTesting(layout);
    ASSERT_NE(compiledLayout, nullptr);
    EXPECT_EQ(
        compiledLayout->status,
        Adapter::DiligentBindingCompileStatus::Compiled);
    EXPECT_EQ(compiledLayout->layoutHandleGeneration, layout.generation);
    EXPECT_EQ(compiledLayout->entries.size(), 2u);
    EXPECT_EQ(compiledLayout->entries[0].shaderVariableName, "g_Albedo");

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
    EXPECT_EQ(backend->GetNativeBindingSetRecordCountForTesting(), 1u);
    const auto* nativeRecord =
        backend->ResolveNativeBindingSetRecordForTesting(bindingSet);
    ASSERT_NE(nativeRecord, nullptr);
    EXPECT_EQ(
        nativeRecord->status,
        Adapter::DiligentBindingCompileStatus::Compiled);
    EXPECT_EQ(nativeRecord->layoutHandleGeneration, layout.generation);
    ASSERT_EQ(nativeRecord->resources.size(), 2u);
    EXPECT_EQ(nativeRecord->resources[0].handle.generation, texture.generation);
    EXPECT_EQ(
        nativeRecord->resources[0].resourceCreationSerial,
        backend->QueryTextureForTesting(texture).creationSerial);

    backend->DestroyBindingSet(bindingSet);
    EXPECT_FALSE(backend->QueryBindingSet(bindingSet).live);
    EXPECT_EQ(backend->GetNativeBindingSetRecordCountForTesting(), 0u);
    backend->DestroyBindingSet(bindingSet);

    backend->DestroyBindingLayout(layout);
    EXPECT_FALSE(backend->QueryBindingLayout(layout).live);
    EXPECT_EQ(backend->GetCompiledBindingLayoutCountForTesting(), 0u);
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
    EXPECT_EQ(backend->GetNativeBindingSetRecordCountForTesting(), 0u);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBindingSetDesc);
}

TEST(GraphicsDiligentBackendAdapter, UnsupportedStorageBufferNeverReachesNativeBindingCompiler)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    EXPECT_FALSE(backend->CreateBindingLayout(MakeStorageBufferLayout()).IsValid());
    EXPECT_EQ(backend->GetCompiledBindingLayoutCountForTesting(), 0u);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBindingLayoutDesc);
}

TEST(GraphicsDiligentBackendAdapter, OptionalBindingRecordsFallbackWithoutNativeFallbackResource)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto layout =
        backend->CreateBindingLayout(MakeOptionalFallbackTextureLayout());
    ASSERT_TRUE(layout.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    const auto bindingSet = backend->CreateBindingSet(set);
    ASSERT_TRUE(bindingSet.IsValid());

    const auto query = backend->QueryBindingSet(bindingSet);
    EXPECT_EQ(query.fallbackRequiredCount, 1u);

    const auto* nativeRecord =
        backend->ResolveNativeBindingSetRecordForTesting(bindingSet);
    ASSERT_NE(nativeRecord, nullptr);
    EXPECT_TRUE(nativeRecord->resources.empty());
    ASSERT_EQ(nativeRecord->fallbackRequirements.size(), 1u);
    EXPECT_TRUE(nativeRecord->fallbackRequirements[0].fallbackRequired);
    EXPECT_EQ(
        nativeRecord->fallbackRequirements[0].shaderVariableName,
        "g_Albedo");
    EXPECT_FALSE(
        nativeRecord->fallbackRequirements[0].handle.IsValid());
}

TEST(FallbackResources, FailedNativeInitializationDoesNotPublishState)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    EXPECT_FALSE(backend->InitializeFallbackResourcesForTesting());

    const auto diagnostics =
        backend->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.fallbackInitializationAttempts, 1u);
    EXPECT_EQ(diagnostics.fallbackInitializationSuccesses, 0u);
    EXPECT_EQ(diagnostics.fallbackInitializationFailures, 1u);
    EXPECT_FALSE(diagnostics.fallbackLiveState);
    EXPECT_EQ(diagnostics.fallbackInternalResourceCount, 0u);
    EXPECT_FALSE(backend->GetFallbackWhiteTextureForTesting().IsValid());
    EXPECT_FALSE(backend->GetFallbackMaterialSamplerForTesting().IsValid());
    EXPECT_EQ(backend->GetResourceMemoryReport().liveTextureCount, 0u);
    EXPECT_EQ(backend->GetResourceMemoryReport().liveSamplerCount, 0u);
}

TEST(FallbackResources, ReleaseIsIdempotentWhenInitializationFailed)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    EXPECT_FALSE(backend->InitializeFallbackResourcesForTesting());
    backend->ReleaseFallbackResourcesForTesting();
    backend->ReleaseFallbackResourcesForTesting();

    const auto diagnostics =
        backend->GetNativeBindingDiagnosticsForTesting();
    EXPECT_FALSE(diagnostics.fallbackLiveState);
    EXPECT_EQ(diagnostics.fallbackInternalResourceCount, 0u);
    EXPECT_EQ(backend->GetResourceMemoryReport().totalLiveBytes, 0u);
}

TEST(GraphicsDiligentBackendAdapter, SameKeyDifferentCanonicalLayoutIsIncompatible)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto layout = backend->CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());

    const auto* compiled =
        backend->ResolveCompiledBindingLayoutForTesting(layout);
    ASSERT_NE(compiled, nullptr);

    auto mutated = *compiled;
    ASSERT_FALSE(mutated.canonicalLayout.slots.empty());
    mutated.canonicalLayout.slots[0].arrayCount += 1u;
    mutated.compatibilityKey = compiled->compatibilityKey;

    EXPECT_FALSE(Adapter::AreDiligentCompiledBindingLayoutsCompatible(
        *compiled,
        mutated));
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

TEST(NativeBindingResolver, DestroyedBindingSetCannotHitCache)
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

    Graphics::PipelineDesc pipelineDesc{};
    pipelineDesc.bindingLayout = layout;
    const auto pipeline = backend->CreatePipeline(pipelineDesc);
    ASSERT_TRUE(pipeline.IsValid());

    const auto bindingSet =
        backend->CreateBindingSet(MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(bindingSet.IsValid());
    backend->DestroyBindingSet(bindingSet);

    EXPECT_EQ(
        backend->ResolveNativeBindingSrbForTesting(pipeline, bindingSet),
        nullptr);
    const auto diagnostics =
        backend->GetNativeBindingDiagnosticsForTesting();
    EXPECT_EQ(diagnostics.staleBindingSetRejects, 1u);
    EXPECT_EQ(diagnostics.nativeBindingCacheHits, 0u);
    EXPECT_EQ(backend->GetNativeBindingCacheEntryCountForTesting(), 0u);
}
