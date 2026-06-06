/// @file GraphicsNullBackendResourceTests.cpp
/// @brief R4A resource validation and accounting tests for NullGraphicsBackend.

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace
{

namespace Graphics = SagaEngine::Graphics;

Graphics::RenderBackendDesc MakeHeadlessDesc() noexcept
{
    Graphics::RenderBackendDesc desc{};
    desc.headless = true;
    return desc;
}

Graphics::TextureDesc MakeTextureDesc() noexcept
{
    Graphics::TextureDesc desc{};
    desc.width = 4u;
    desc.height = 4u;
    desc.format = Graphics::ResourceFormat::Rgba8Unorm;
    return desc;
}

Graphics::BufferDesc MakeBufferDesc() noexcept
{
    Graphics::BufferDesc desc{};
    desc.sizeBytes = 128u;
    return desc;
}

Graphics::ShaderDesc MakeShaderDesc() noexcept
{
    Graphics::ShaderDesc desc{};
    desc.byteSize = 32u;
    return desc;
}

template <std::size_t N>
Graphics::GraphicsDataView MakeDataView(
    const std::array<std::uint8_t, N>& data) noexcept
{
    return {data.data(), static_cast<std::uint64_t>(data.size()), 0u, 0u};
}

TEST(GraphicsNullBackendResources, ValidatesTextureAndBufferDescriptors)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    auto texture = MakeTextureDesc();
    texture.width = 0u;
    EXPECT_FALSE(backend.CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    texture = MakeTextureDesc();
    texture.format = Graphics::ResourceFormat::Unknown;
    EXPECT_FALSE(backend.CreateTexture(texture).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);

    Graphics::BufferDesc buffer{};
    EXPECT_FALSE(backend.CreateBuffer(buffer).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);

    const auto report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.liveBufferCount, 0u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_EQ(report.failedCreateCount, 3u);
}

TEST(GraphicsNullBackendResources, RejectsInvalidDescriptions)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::ShaderDesc shader{};
    EXPECT_FALSE(backend.CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);

    shader = MakeShaderDesc();
    shader.stage = static_cast<Graphics::ShaderStage>(255);
    EXPECT_FALSE(backend.CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);

    Graphics::PipelineDesc pipeline{};
    pipeline.colorTargetCount = 0u;
    EXPECT_FALSE(backend.CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    pipeline = {};
    pipeline.topology = static_cast<Graphics::PrimitiveTopology>(255);
    EXPECT_FALSE(backend.CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);

    Graphics::SamplerDesc sampler{};
    sampler.minFilter = static_cast<Graphics::FilterMode>(255);
    EXPECT_FALSE(backend.CreateSampler(sampler).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);

    EXPECT_EQ(backend.GetResourceMemoryReport().failedCreateCount, 5u);
}

TEST(GraphicsNullBackendResources, TracksLiveAndPeakMemory)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    const auto shader = backend.CreateShader(MakeShaderDesc());
    const auto pipeline = backend.CreatePipeline({});
    const auto sampler = backend.CreateSampler({});

    EXPECT_TRUE(texture.IsValid());
    EXPECT_TRUE(buffer.IsValid());
    EXPECT_TRUE(shader.IsValid());
    EXPECT_TRUE(pipeline.IsValid());
    EXPECT_TRUE(sampler.IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);

    auto report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.liveBufferCount, 1u);
    EXPECT_EQ(report.liveShaderCount, 1u);
    EXPECT_EQ(report.livePipelineCount, 1u);
    EXPECT_EQ(report.liveSamplerCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(report.bufferBytes, 128u);
    EXPECT_EQ(report.shaderBytes, 32u);
    EXPECT_EQ(report.pipelineBytes, 0u);
    EXPECT_EQ(report.samplerBytes, 0u);
    EXPECT_EQ(report.totalLiveBytes, 224u);
    EXPECT_EQ(report.peakLiveBytes, 224u);

    backend.DestroyBuffer(buffer);
    report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.liveBufferCount, 0u);
    EXPECT_EQ(report.bufferBytes, 0u);
    EXPECT_EQ(report.totalLiveBytes, 96u);
    EXPECT_EQ(report.peakLiveBytes, 224u);
}

TEST(GraphicsNullBackendResources, StaleDoubleAndInvalidDestroyAreNoOps)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto first = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(first.IsValid());
    backend.DestroyTexture(first);

    const auto afterFirstDestroy = backend.GetResourceMemoryReport();
    EXPECT_EQ(afterFirstDestroy.liveTextureCount, 0u);
    EXPECT_EQ(afterFirstDestroy.totalLiveBytes, 0u);
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);

    backend.DestroyTexture({});
    backend.DestroyTexture(first);
    backend.DestroyTexture(first);

    const auto afterNoOps = backend.GetResourceMemoryReport();
    EXPECT_EQ(afterNoOps.liveTextureCount, 0u);
    EXPECT_EQ(afterNoOps.totalLiveBytes, 0u);
    EXPECT_EQ(afterNoOps.failedCreateCount, 0u);
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);

    const auto second = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(second.index, first.index);
    EXPECT_EQ(second.generation, first.generation + 1u);

    backend.DestroyTexture(first);
    EXPECT_EQ(backend.GetResourceMemoryReport().liveTextureCount, 1u);
}

TEST(GraphicsNullBackendResources, ShutdownClearsLiveResources)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    EXPECT_TRUE(backend.CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_TRUE(backend.CreateBuffer(MakeBufferDesc()).IsValid());
    ASSERT_NE(backend.GetResourceMemoryReport().totalLiveBytes, 0u);

    backend.Shutdown();

    const auto leakSummary = backend.GetLastShutdownResourceLeakSummary();
    EXPECT_TRUE(leakSummary.hadLiveResources);
    EXPECT_EQ(leakSummary.leakedTextureCount, 1u);
    EXPECT_EQ(leakSummary.leakedBufferCount, 1u);
    EXPECT_EQ(leakSummary.textureBytes, 64u);
    EXPECT_EQ(leakSummary.bufferBytes, 128u);
    EXPECT_EQ(leakSummary.totalLeakedBytes, 192u);

    const auto report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 0u);
    EXPECT_EQ(report.liveBufferCount, 0u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
    EXPECT_NE(report.peakLiveBytes, 0u);
    EXPECT_EQ(backend.GetStatus().health, Graphics::RenderBackendHealth::Shutdown);

    backend.DestroyTexture({});
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
}

TEST(GraphicsNullBackendResources, DestroyNoOpsDoNotUpdateFailure)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend.DestroyTexture(texture);

    auto replacement = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(replacement.IsValid());
    ASSERT_NE(replacement.generation, texture.generation);
    backend.DestroyTexture(texture);
    backend.DestroyTexture({});
    backend.DestroyTexture(replacement);
    backend.DestroyTexture(replacement);
    backend.Shutdown();
    backend.DestroyTexture(replacement);

    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
    EXPECT_EQ(backend.GetResourceMemoryReport().failedCreateCount, 0u);
}

TEST(GraphicsNullBackendResources, RegisteredHandlesReportBackingState)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    EXPECT_EQ(
        backend.GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(
        backend.GetBufferBackingForTesting(buffer),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(
        backend.GetTextureBackingForTesting({}),
        Graphics::GraphicsResourceBacking::Invalid);

    backend.DestroyTexture(texture);
    EXPECT_EQ(
        backend.GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::Invalid);

    const auto nextTexture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(nextTexture.IsValid());
    backend.Shutdown();
    EXPECT_EQ(
        backend.GetTextureBackingForTesting(nextTexture),
        Graphics::GraphicsResourceBacking::Invalid);
}

TEST(GraphicsNullBackendResources, QueryHelpersReportLiveKindAndBytes)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    const auto shader = backend.CreateShader(MakeShaderDesc());
    const auto pipeline = backend.CreatePipeline({});
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());
    ASSERT_TRUE(shader.IsValid());
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    auto query = backend.QueryTextureForTesting(texture);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(query.approximateBytes, 64u);

    query = backend.QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.approximateBytes, 128u);

    query = backend.QueryShaderForTesting(shader);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Shader);
    EXPECT_EQ(query.approximateBytes, 32u);

    query = backend.QueryPipelineForTesting(pipeline);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Pipeline);
    EXPECT_EQ(query.approximateBytes, 0u);

    query = backend.QuerySamplerForTesting(sampler);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Sampler);
    EXPECT_EQ(query.approximateBytes, 0u);

    backend.DestroyBuffer(buffer);
    query = backend.QueryBufferForTesting(buffer);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::Invalid);

    backend.Shutdown();
    query = backend.QueryTextureForTesting(texture);
    EXPECT_FALSE(query.live);
    EXPECT_EQ(query.approximateBytes, 0u);
}

TEST(GraphicsNullBackendResources, CreateRequiresInitializedBackend)
{
    Graphics::NullGraphicsBackend backend;

    EXPECT_FALSE(backend.CreateTexture(MakeTextureDesc()).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::BackendNotInitialized);

    const auto report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.failedCreateCount, 1u);
    EXPECT_EQ(report.totalLiveBytes, 0u);
}

TEST(GraphicsNullBackendResources, InitialTextureDataRequiresFullMip0Payload)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend.CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());

    const auto report = backend.GetResourceMemoryReport();
    EXPECT_EQ(report.liveTextureCount, 1u);
    EXPECT_EQ(report.textureBytes, 64u);
    EXPECT_EQ(report.totalLiveBytes, 64u);
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
}

TEST(GraphicsNullBackendResources, RejectsPartialTextureInitialData)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 63> pixels{};
    EXPECT_FALSE(
        backend.CreateTexture(MakeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
    EXPECT_EQ(backend.GetResourceMemoryReport().totalLiveBytes, 0u);
}

TEST(GraphicsNullBackendResources, RejectsOversizedTextureInitialData)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 65> pixels{};
    EXPECT_FALSE(
        backend.CreateTexture(MakeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsNullBackendResources, RejectsInvalidTexturePitch)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 64> pixels{};
    Graphics::GraphicsDataView rowPitchTooSmall{
        pixels.data(),
        static_cast<std::uint64_t>(pixels.size()),
        15u,
        0u,
    };
    EXPECT_FALSE(
        backend.CreateTexture(MakeTextureDesc(), rowPitchTooSmall).IsValid());

    Graphics::GraphicsDataView slicePitchTooSmall{
        pixels.data(),
        static_cast<std::uint64_t>(pixels.size()),
        16u,
        63u,
    };
    EXPECT_FALSE(
        backend.CreateTexture(MakeTextureDesc(), slicePitchTooSmall).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsNullBackendResources, RejectsNullPointerWithNonZeroInitialDataSize)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::GraphicsDataView invalidView{};
    invalidView.sizeBytes = 64u;
    EXPECT_FALSE(
        backend.CreateTexture(MakeTextureDesc(), invalidView).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsNullBackendResources, EmptyInitialDataWithNonNullPointerIsValid)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 1> byte{};
    Graphics::GraphicsDataView emptyView{byte.data(), 0u, 0u, 0u};
    EXPECT_TRUE(backend.CreateTexture(MakeTextureDesc(), emptyView).IsValid());
    EXPECT_TRUE(backend.CreateBuffer(MakeBufferDesc(), emptyView).IsValid());
}

TEST(GraphicsNullBackendResources, InitialBufferDataRejectsOversizedPayload)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 128> validBytes{};
    EXPECT_TRUE(
        backend.CreateBuffer(MakeBufferDesc(), MakeDataView(validBytes))
            .IsValid());

    std::array<std::uint8_t, 129> oversizedBytes{};
    EXPECT_FALSE(
        backend.CreateBuffer(MakeBufferDesc(), MakeDataView(oversizedBytes))
            .IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsNullBackendResources, DestroyAndShutdownClearInitialDataResources)
{
    Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    std::array<std::uint8_t, 64> pixels{};
    const auto texture =
        backend.CreateTexture(MakeTextureDesc(), MakeDataView(pixels));
    ASSERT_TRUE(texture.IsValid());
    backend.DestroyTexture(texture);
    EXPECT_EQ(backend.GetResourceMemoryReport().liveTextureCount, 0u);

    EXPECT_TRUE(
        backend.CreateTexture(MakeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    backend.Shutdown();
    EXPECT_EQ(backend.GetResourceMemoryReport().liveTextureCount, 0u);
    EXPECT_EQ(backend.GetResourceMemoryReport().totalLiveBytes, 0u);
}

} // namespace
