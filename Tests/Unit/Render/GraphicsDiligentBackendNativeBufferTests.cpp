/// @file GraphicsDiligentBackendNativeBufferTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, NativePreparedHandlesReportBackingState)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(
        backend->GetBufferBackingForTesting(buffer),
        Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);
    EXPECT_EQ(backend->GetBufferNativeSerialForTesting(buffer), 0u);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting({}),
        Graphics::GraphicsResourceBacking::Invalid);

    backend->DestroyTexture(texture);
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(texture),
        Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(texture), 0u);

    const auto nextTexture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(nextTexture.IsValid());
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    backend->Shutdown();
    EXPECT_EQ(
        backend->GetTextureBackingForTesting(nextTexture),
        Graphics::GraphicsResourceBacking::Invalid);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(nextTexture), 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsZeroSize)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex);
    desc.sizeBytes = 0u;

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsOverflow)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    auto desc = MakeNativeBufferDesc(
        Graphics::BufferUsage::Vertex,
        std::numeric_limits<std::uint64_t>::max());

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsOversizedPayload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 16u> bytes{};
    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex, 8u);

    EXPECT_FALSE(
        backend->CreateBuffer(desc, MakeDataView(bytes)).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRejectsInvalidUsage)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Storage);

    EXPECT_FALSE(backend->CreateBuffer(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBufferDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferRequiresBoundServices)
{
    RenderBackend::DiligentNativeResourceOwner owner;
    std::string diagnostic;
    const auto serial = owner.CreateBufferForHandle(
        MakeBufferHandle(1u, 1001u),
        MakeNativeBufferDesc(Graphics::BufferUsage::Vertex),
        {},
        &diagnostic);

    EXPECT_EQ(serial, 0u);
    EXPECT_FALSE(diagnostic.empty());
}

TEST(GraphicsDiligentBackendAdapter, FailedNativeCreationReturnsInvalidHandle)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    auto desc = MakeNativeBufferDesc(
        Graphics::BufferUsage::Vertex,
        std::numeric_limits<std::uint64_t>::max());
    const auto buffer = backend->CreateBuffer(desc);

    EXPECT_FALSE(buffer.IsValid());
    EXPECT_EQ(backend->GetResourceMemoryReport().liveBufferCount, 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Vertex);
    desc.debugName = "query-native-buffer";
    const auto buffer = backend->CreateBuffer(desc);
    ASSERT_TRUE(buffer.IsValid());

    ASSERT_TRUE(backend->MarkBufferNativeForTesting(buffer, 42u));

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.logicalBytes, desc.sizeBytes);
    EXPECT_EQ(query.debugName, "query-native-buffer");
}

TEST(GraphicsDiligentBackendAdapter, WrongKindBufferQueryIsRejected)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    const auto query = backend->QueryResource(
        Graphics::GraphicsResourceKind::Texture,
        buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Invalid);
}

TEST(GraphicsDiligentBackendAdapter, StaleBufferQueryIsRejected)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());
    backend->DestroyBuffer(buffer);

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_FALSE(query.valid);
    EXPECT_FALSE(query.live);
}

TEST(GraphicsDiligentBackendAdapter, NativeBufferCarriesDebugName)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeBufferDesc(Graphics::BufferUsage::Uniform);
    desc.debugName = "camera-constants";
    const auto buffer = backend->CreateBuffer(desc);
    ASSERT_TRUE(buffer.IsValid());

    const auto query = backend->QueryBufferForTesting(buffer);
    EXPECT_EQ(query.debugName, "camera-constants");
}

TEST(GraphicsDiligentBackendAdapter, ForcedReadyUnitBackendStaysRegisteredOnly)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    EXPECT_FALSE(backend->HasNativeDeviceServicesForTesting());

    const auto unboundTexture = backend->CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(unboundTexture.IsValid());

    auto query = backend->QueryTextureForTesting(unboundTexture);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(
        query.lifecycle,
        Graphics::GraphicsResourceLifecycle::RegisteredOnly);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_FALSE(query.nativeBacked);
    EXPECT_EQ(query.logicalBytes, 64u);
    EXPECT_EQ(query.approximateBytes, query.logicalBytes);
    EXPECT_FALSE(query.resident);
    EXPECT_FALSE(query.pendingDestroy);
    EXPECT_NE(query.creationSerial, 0u);
    EXPECT_EQ(query.lastUseSerial, 0u);
    EXPECT_EQ(backend->GetTextureNativeSerialForTesting(unboundTexture), 0u);

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    query = backend->QueryBufferForTesting(buffer);
    EXPECT_TRUE(query.valid);
    EXPECT_TRUE(query.live);
    EXPECT_EQ(query.kind, Graphics::GraphicsResourceKind::Buffer);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::RegisteredOnly);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::RegisteredOnly);
    EXPECT_FALSE(query.nativeBacked);
    EXPECT_EQ(query.logicalBytes, 128u);
    EXPECT_FALSE(query.resident);
    EXPECT_NE(query.creationSerial, 0u);
    EXPECT_EQ(backend->GetBufferNativeSerialForTesting(buffer), 0u);
}

TEST(GraphicsDiligentBackendAdapter, NativeBackendDoesNotCreateSecondDevice)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    EXPECT_EQ(state.initializeCalls, 0u);

    const auto texture = backend->CreateTexture(MakeTextureDesc());
    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    EXPECT_EQ(state.initializeCalls, 0u);
    EXPECT_EQ(state.textureCreateCalls, 0u);
    EXPECT_EQ(state.beginFrameCalls, 0u);
    EXPECT_EQ(state.endFrameCalls, 0u);
}
