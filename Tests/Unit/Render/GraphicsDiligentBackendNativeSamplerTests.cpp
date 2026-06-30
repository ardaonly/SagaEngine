/// @file GraphicsDiligentBackendNativeSamplerTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, NativeSamplerRejectsInvalidFilter)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakePointClampSamplerDesc();
    desc.minFilter = static_cast<Graphics::FilterMode>(255);

    EXPECT_FALSE(backend->CreateSampler(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeSamplerRejectsInvalidAddressMode)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakePointClampSamplerDesc();
    desc.addressU = static_cast<Graphics::AddressMode>(255);

    EXPECT_FALSE(backend->CreateSampler(desc).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidSamplerDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeSamplerRequiresBoundServices)
{
    RenderBackend::DiligentNativeResourceOwner owner;
    std::string diagnostic;

    const auto serial = owner.CreateSamplerForHandle(
        ToSamplerHandle({1u, 4001u}),
        MakePointClampSamplerDesc(),
        &diagnostic);

    EXPECT_EQ(serial, 0u);
    EXPECT_FALSE(diagnostic.empty());
}

TEST(GraphicsDiligentBackendAdapter, NativeSamplerQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto desc = MakePointClampSamplerDesc();
    desc.debugName = "query-native-sampler";
    const auto sampler = backend->CreateSampler(desc);
    ASSERT_TRUE(sampler.IsValid());
    ASSERT_TRUE(backend->MarkSamplerNativeForTesting(sampler, 88u));

    const auto query = backend->QuerySamplerForTesting(sampler);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.debugName, "query-native-sampler");
}

TEST(GraphicsDiligentBackendAdapter, DestroyedSamplerQueryIsInvalid)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto sampler = backend->CreateSampler(MakePointClampSamplerDesc());
    ASSERT_TRUE(sampler.IsValid());
    backend->DestroySampler(sampler);

    EXPECT_FALSE(backend->QuerySamplerForTesting(sampler).valid);
}

TEST(GraphicsDiligentBackendAdapter, StaleSamplerQueryIsInvalid)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto sampler = backend->CreateSampler(MakePointClampSamplerDesc());
    ASSERT_TRUE(sampler.IsValid());
    backend->DestroySampler(sampler);
    const auto replacement = backend->CreateSampler(MakePointClampSamplerDesc());
    ASSERT_TRUE(replacement.IsValid());

    EXPECT_FALSE(backend->QuerySamplerForTesting(sampler).valid);
    EXPECT_TRUE(backend->QuerySamplerForTesting(replacement).valid);
}
