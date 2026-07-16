/// @file GraphicsDiligentBackendResourceRegistryTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, ResourceRegistryRejectsStaleHandles)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto first = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(first.IsValid());
    EXPECT_EQ(first.index, 1u);
    EXPECT_EQ(first.generation, 1u);

    backend->DestroyTexture(first);

    const auto second = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(second.index, first.index);
    EXPECT_EQ(second.generation, first.generation + 1u);

    backend->DestroyTexture(first);
    backend->DestroyTexture(second);
    backend->DestroyTexture(second);

    const auto third = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(third.IsValid());
    EXPECT_EQ(third.index, second.index);
    EXPECT_EQ(third.generation, second.generation + 1u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceValidationRejectsInvalidDescriptions)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto texture = MakeTextureDesc();
    texture.width = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    Graphics::BufferDesc buffer{};
    EXPECT_FALSE(backend->CreateBuffer(buffer).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);

    Graphics::ShaderDesc shader{};
    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);

    Graphics::PipelineDesc pipeline{};
    pipeline.colorTargetCount = 0u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    Graphics::SamplerDesc sampler{};
    sampler.addressU = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.failedCreateCount, 5u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceValidationRejectsRangeAndEnumEdges)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto texture = MakeTextureDesc();
    texture.depth = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    texture = MakeTextureDesc();
    texture.mipLevels = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    texture = MakeTextureDesc();
    texture.arrayLayers = 0u;
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    texture = MakeTextureDesc();
    texture.format = static_cast<Graphics::ResourceFormat>(255);
    EXPECT_FALSE(backend->CreateTexture(texture).IsValid());

    auto pipeline = Graphics::PipelineDesc{};
    pipeline.colorTargetCount = 9u;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    pipeline = {};
    pipeline.colorFormat = static_cast<Graphics::ResourceFormat>(255);
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());

    pipeline = {};
    pipeline.depthFormat = Graphics::ResourceFormat::Unknown;
    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());

    auto sampler = Graphics::SamplerDesc{};
    sampler.magFilter = static_cast<Graphics::FilterMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);

    sampler = {};
    sampler.addressV = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());

    sampler = {};
    sampler.addressW = static_cast<Graphics::AddressMode>(255);
    EXPECT_FALSE(backend->CreateSampler(sampler).IsValid());

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.failedCreateCount, 10u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, FailedCreateUpdatesLastFailure)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);

    EXPECT_FALSE(backend->CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::BackendNotInitialized);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 1u);

    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    Graphics::ShaderDesc shader{};
    shader.stage = static_cast<Graphics::ShaderStage>(255);
    shader.byteSize = 32u;
    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 2u);
}

TEST(GraphicsDiligentBackendAdapter, InitializedCreateReturnsNativePreparedHandle)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    const auto shader = backend->CreateShader(MakeShaderDesc());
    const auto pipeline = backend->CreatePipeline({});
    const auto sampler = backend->CreateSampler({});

    EXPECT_TRUE(texture.IsValid());
    EXPECT_TRUE(buffer.IsValid());
    EXPECT_TRUE(shader.IsValid());
    EXPECT_TRUE(pipeline.IsValid());
    EXPECT_TRUE(sampler.IsValid());
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
}

TEST(GraphicsDiligentBackendAdapter, ResourceMemoryReportChangesOnCreateDestroy)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    const auto shader = backend->CreateShader(MakeShaderDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());
    ASSERT_TRUE(shader.IsValid());

    auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.liveBufferCount, 1u);
    EXPECT_EQ(report.liveShaderCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(report.bufferBytes, 128u);
    EXPECT_EQ(report.shaderBytes, 32u);
    EXPECT_EQ(report.totalLiveBytes, 224u);
    EXPECT_EQ(report.peakLiveBytes, 224u);
    EXPECT_EQ(report.pendingDestroyBytes, 0u);
    EXPECT_EQ(report.uploadReservedBytes, 0u);
    EXPECT_EQ(report.uploadUsedBytes, 0u);
    EXPECT_EQ(report.uploadInFlightBytes, 0u);
    EXPECT_EQ(report.nativeCommittedBytes, 0u);
    EXPECT_FALSE(report.nativeCommittedBytesKnown);

    backend->DestroyTexture(texture);
    report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.textureBytes, 0u);
    EXPECT_EQ(report.totalLiveBytes, 160u);
    EXPECT_EQ(report.peakLiveBytes, 224u);

    backend->DestroyTexture(texture);
    backend->DestroyTexture({});
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
    EXPECT_EQ(backend->GetResourceMemoryReport().totalLiveBytes, 160u);
}

TEST(GraphicsDiligentBackendAdapter, ShutdownClearsResourceReport)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    EXPECT_TRUE(backend->CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_TRUE(backend->CreateBuffer(MakeBufferDesc()).IsValid());
    ASSERT_NE(backend->GetResourceMemoryReport().totalLiveBytes, 0u);

    backend->Shutdown();

    const auto leakSummary = backend->GetLastShutdownResourceLeakSummary();
    EXPECT_TRUE(leakSummary.hadLiveResources);
    EXPECT_EQ(leakSummary.leakedTextureCount, 1u);
    EXPECT_EQ(leakSummary.leakedBufferCount, 1u);
    EXPECT_EQ(leakSummary.textureBytes, 64u);
    EXPECT_EQ(leakSummary.bufferBytes, 128u);
    EXPECT_EQ(leakSummary.totalLeakedBytes, 192u);

    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.liveBufferCount, 0u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_NE(report.peakLiveBytes, 0u);
    EXPECT_EQ(
        backend->GetStatus().health,
        Graphics::RenderBackendHealth::Shutdown);
}

TEST(GraphicsDiligentBackendAdapter, DestroyNoOpsDoNotUpdateFailure)
{
    FakeRenderState state;
    auto backend = MakeBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend->DestroyTexture(texture);

    const auto replacement = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(replacement.IsValid());
    ASSERT_NE(replacement.generation, texture.generation);
    backend->DestroyTexture(texture);
    backend->DestroyTexture({});
    backend->DestroyTexture(replacement);
    backend->DestroyTexture(replacement);
    backend->Shutdown();
    backend->DestroyTexture(replacement);

    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 0u);
}

TEST(GraphicsDiligentBackendAdapter, ResourceQueriesRejectWrongKindAndStale)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    auto query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Invalid);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::Invalid);

    backend->DestroyTexture(texture);
    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        texture);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.logicalBytes, 0u);
    EXPECT_FALSE(query.pendingDestroy);

    const auto failed = backend->CreateBuffer({});
    EXPECT_FALSE(failed.IsValid());
    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Buffer,
        failed);
    EXPECT_FALSE(query.valid);
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, QueryHelpersReportLiveKindAndBytes)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto textureDesc = MakeTextureDesc();
    textureDesc.debugName = "albedo";
    auto bufferDesc = MakeBufferDesc();
    bufferDesc.debugName = "vertices";
    auto shaderDesc = MakeShaderDesc();
    shaderDesc.debugName = "solid-vs";
    Graphics::PipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "opaque-pipeline";
    Graphics::SamplerDesc samplerDesc{};
    samplerDesc.debugName = "linear-clamp";

    const auto texture = backend->CreateTexture(textureDesc);
    const auto buffer = backend->CreateBuffer(bufferDesc);
    const auto shader = backend->CreateShader(shaderDesc);
    const auto pipeline = backend->CreatePipeline(pipelineDesc);
    const auto sampler = backend->CreateSampler(samplerDesc);
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());
    ASSERT_TRUE(shader.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    auto query = backend->QueryTextureForTesting(texture);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 64u);
    EXPECT_EQ(query.debugName, "albedo");

    query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        texture);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(query.debugName, "albedo");

    query = backend->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 128u);
    EXPECT_EQ(query.debugName, "vertices");

    query = backend->QueryShaderForTesting(shader);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Shader);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 32u);
    EXPECT_EQ(query.debugName, "solid-vs");

    query = backend->QueryPipelineForTesting(pipeline);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Pipeline);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(query.debugName, "opaque-pipeline");

    query = backend->QuerySamplerForTesting(sampler);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Sampler);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(query.debugName, "linear-clamp");
    EXPECT_EQ(backend->GetSamplerNativeSerialForTesting(sampler), 0u);

    backend->DestroyBuffer(buffer);
    query = backend->QueryBufferForTesting(buffer);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_TRUE(query.debugName.empty());

    backend->Shutdown();
    query = backend->QueryTextureForTesting(texture);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.approximateBytes, 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, HeadlessResourceReportUsesRegisteredOnlyHandles)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);

    Graphics::RenderBackendDesc desc{};
    desc.headless = true;

    EXPECT_TRUE(backend->Initialize(desc, {}));
    EXPECT_EQ(state.initializeCalls, 0u);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    const auto report = backend->GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);
    EXPECT_EQ(state.initializeCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, InitialDataAcceptedWithoutNativeUpload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(texture), 64u);
    EXPECT_FALSE(backend->GetTextureNativeUploadDeferredForTesting(texture));
    EXPECT_EQ(state.textureCreateCalls, 0u);

    std::array<std::uint8_t, 128> bytes{};
    const auto buffer =
        backend->CreateBuffer(MakeBufferDesc(), MakeDataView(bytes));
    ASSERT_TRUE(buffer.IsValid());
    EXPECT_EQ(backend->GetBufferShadowBytesForTesting(buffer), 128u);
    EXPECT_FALSE(backend->GetBufferNativeUploadDeferredForTesting(buffer));
}

TEST(GraphicsDiligentBackendAdapter, InvalidInitialDataUpdatesLastFailure)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 63> partialPixels{};
    EXPECT_FALSE(
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(partialPixels))
            .IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);

    Graphics::GraphicsDataView nullPayload{};
    nullPayload.sizeBytes = 1u;
    EXPECT_FALSE(
        backend->CreateBuffer(MakeBufferDesc(), nullPayload).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
    EXPECT_EQ(backend->GetResourceMemoryReport().failedCreateCount, 2u);
}

TEST(GraphicsDiligentBackendAdapter, InitialDataDoesNotTouchRenderBackendUploadPath)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 64> pixels{};
    EXPECT_TRUE(
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    EXPECT_EQ(state.textureCreateCalls, 0u);
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, DestroyAndShutdownReleaseShadowPayload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());
    ASSERT_EQ(backend->GetTextureShadowBytesForTesting(texture), 64u);

    backend->DestroyTexture(texture);
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(texture), 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);

    const auto nextTexture =
        backend->CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(nextTexture.IsValid());
    ASSERT_EQ(backend->GetTextureShadowBytesForTesting(nextTexture), 64u);
    ASSERT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);

    backend->Shutdown();
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(nextTexture), 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    EXPECT_EQ(backend->GetResourceMemoryReport().totalLiveBytes, 0u);
}
