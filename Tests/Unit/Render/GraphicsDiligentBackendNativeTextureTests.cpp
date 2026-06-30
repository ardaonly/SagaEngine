/// @file GraphicsDiligentBackendNativeTextureTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsZeroWidth)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.width = 0u;

    EXPECT_FALSE(backend->CreateTexture(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsZeroHeight)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.height = 0u;

    EXPECT_FALSE(backend->CreateTexture(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsOverflow)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.width = std::numeric_limits<std::uint32_t>::max();
    desc.height = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint8_t, 4u> pixels{};

    EXPECT_FALSE(backend->CreateTexture(desc, MakeDataView(pixels)).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsUnsupportedFormat)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.format = Graphics::ResourceFormat::Bgra8Unorm;

    EXPECT_FALSE(backend->CreateTexture(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsUnsupportedUsage)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.usage = Graphics::TextureUsageFlags::RenderTarget;

    EXPECT_FALSE(backend->CreateTexture(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidTextureDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsNullPayload)
{
    RenderBackend::DiligentNativeResourceOwner owner;
    std::string diagnostic;
    const auto serial = owner.CreateTextureForHandle(
        ToTextureHandle({1u, 1u}),
        MakeNativeTextureDesc(),
        {},
        &diagnostic);

    EXPECT_EQ(serial, 0u);
    EXPECT_FALSE(diagnostic.empty());
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsShortRowPitch)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 64u> pixels{};
    auto view = MakeDataView(pixels);
    view.rowPitchBytes = 8u;

    EXPECT_FALSE(
        backend->CreateTexture(MakeNativeTextureDesc(), view).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRejectsInsufficientPayload)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 63u> pixels{};
    EXPECT_FALSE(
        backend->CreateTexture(MakeNativeTextureDesc(), MakeDataView(pixels))
            .IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidInitialData);
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureRequiresBoundServices)
{
    RenderBackend::DiligentNativeResourceOwner owner;
    std::array<std::uint8_t, 64u> pixels{};
    std::string diagnostic;

    const auto serial = owner.CreateTextureForHandle(
        ToTextureHandle({1u, 1u}),
        MakeNativeTextureDesc(),
        MakeDataView(pixels),
        &diagnostic);

    EXPECT_EQ(serial, 0u);
    EXPECT_FALSE(diagnostic.empty());
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakeNativeTextureDesc();
    desc.debugName = "query-native-texture";
    const auto texture = backend->CreateTexture(desc);
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(backend->MarkTextureNativeForTesting(texture, 77u));

    const auto query = backend->QueryTextureForTesting(texture);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.logicalBytes, 64u);
    EXPECT_EQ(query.debugName, "query-native-texture");
}

TEST(GraphicsDiligentBackendAdapter, NativeTextureAcceptsLinearAndSrgbFormats)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    std::array<std::uint8_t, 64u> pixels{};
    auto linear = MakeNativeTextureDesc();
    auto srgb = MakeNativeTextureDesc();
    srgb.format = Graphics::ResourceFormat::Rgba8UnormSrgb;

    const auto linearHandle =
        backend->CreateTexture(linear, MakeDataView(pixels));
    const auto srgbHandle =
        backend->CreateTexture(srgb, MakeDataView(pixels));

    EXPECT_TRUE(linearHandle.IsValid());
    EXPECT_TRUE(srgbHandle.IsValid());
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(linearHandle), 64u);
    EXPECT_EQ(backend->GetTextureShadowBytesForTesting(srgbHandle), 64u);
}

TEST(GraphicsDiligentBackendAdapter, DestroyedTextureQueryIsInvalid)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeNativeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend->DestroyTexture(texture);

    EXPECT_FALSE(backend->QueryTextureForTesting(texture).valid);
}

TEST(GraphicsDiligentBackendAdapter, StaleTextureQueryIsInvalid)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto texture = backend->CreateTexture(MakeNativeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend->DestroyTexture(texture);
    const auto replacement = backend->CreateTexture(MakeNativeTextureDesc());
    ASSERT_TRUE(replacement.IsValid());

    EXPECT_FALSE(backend->QueryTextureForTesting(texture).valid);
    EXPECT_TRUE(backend->QueryTextureForTesting(replacement).valid);
}

TEST(GraphicsDiligentBackendAdapter, WrongKindTextureQueryIsInvalid)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto buffer = backend->CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());
    EXPECT_FALSE(
        backend->QueryTextureForTesting(ToTextureHandle(buffer)).valid);
}
