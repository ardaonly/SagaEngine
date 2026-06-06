/// @file GraphicsBindingValidationTests.cpp
/// @brief CPU-side graphics binding validation tests.

#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <gtest/gtest.h>

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

Graphics::GraphicsHandle ToGraphicsHandle(
    const Graphics::GraphicsHandle& handle) noexcept
{
    return handle;
}

Graphics::TextureHandle ToTextureHandle(
    Graphics::GraphicsHandle handle) noexcept
{
    Graphics::TextureHandle texture{};
    texture.index = handle.index;
    texture.generation = handle.generation;
    return texture;
}

Graphics::BufferHandle ToBufferHandle(
    Graphics::GraphicsHandle handle) noexcept
{
    Graphics::BufferHandle buffer{};
    buffer.index = handle.index;
    buffer.generation = handle.generation;
    return buffer;
}

Graphics::SamplerHandle ToSamplerHandle(
    Graphics::GraphicsHandle handle) noexcept
{
    Graphics::SamplerHandle sampler{};
    sampler.index = handle.index;
    sampler.generation = handle.generation;
    return sampler;
}

Graphics::GraphicsResourceQueryResult QueryNullBackendResource(
    void* userData,
    Graphics::GraphicsResourceKind kind,
    Graphics::GraphicsHandle handle)
{
    auto& backend =
        *static_cast<Graphics::NullGraphicsBackend*>(userData);

    switch (kind)
    {
    case Graphics::GraphicsResourceKind::Texture:
        return backend.QueryTextureForTesting(ToTextureHandle(handle));
    case Graphics::GraphicsResourceKind::Buffer:
        return backend.QueryBufferForTesting(ToBufferHandle(handle));
    case Graphics::GraphicsResourceKind::Sampler:
        return backend.QuerySamplerForTesting(ToSamplerHandle(handle));
    default:
        return {};
    }
}

Graphics::GraphicsBindingLayoutDesc MakeTextureSamplerLayout()
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.slots.push_back({
        0u,
        Graphics::GraphicsBindingType::Texture,
        Graphics::kGraphicsShaderStageFragment,
        true,
        1u,
    });
    layout.slots.push_back({
        1u,
        Graphics::GraphicsBindingType::Sampler,
        Graphics::kGraphicsShaderStageFragment,
        false,
        Graphics::kInvalidGraphicsBindingSlot,
    });
    return layout;
}

Graphics::GraphicsBindingResourceRef MakeTextureBinding(
    Graphics::TextureHandle handle) noexcept
{
    return {
        0u,
        Graphics::GraphicsResourceKind::Texture,
        ToGraphicsHandle(handle),
    };
}

Graphics::GraphicsBindingResourceRef MakeSamplerBinding(
    Graphics::SamplerHandle handle) noexcept
{
    return {
        1u,
        Graphics::GraphicsResourceKind::Sampler,
        ToGraphicsHandle(handle),
    };
}

} // namespace

TEST(GraphicsBindingValidation, ValidTextureAndSamplerBindingPasses)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));
    bindingSet.resources.push_back(MakeSamplerBinding(sampler));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.code, Graphics::GraphicsBindingValidationCode::None);
}

TEST(GraphicsBindingValidation, DuplicateSlotRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));
    bindingSet.resources.push_back(MakeTextureBinding(texture));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::DuplicateBindingSlot);
}

TEST(GraphicsBindingValidation, DuplicateLayoutSlotRejected)
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.slots.push_back({
        0u,
        Graphics::GraphicsBindingType::Texture,
        Graphics::kGraphicsShaderStageFragment,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
    });
    layout.slots.push_back({
        0u,
        Graphics::GraphicsBindingType::Buffer,
        Graphics::kGraphicsShaderStageVertex,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
    });

    const auto result = Graphics::ValidateGraphicsBindingSet(
        layout,
        {},
        nullptr,
        nullptr);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::DuplicateLayoutSlot);
    EXPECT_EQ(result.slot, 0u);
}

TEST(GraphicsBindingValidation, MissingRequiredBindingRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        {},
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::MissingRequiredBinding);
    EXPECT_EQ(result.slot, 0u);
}

TEST(GraphicsBindingValidation, OptionalSamplerCanBeOmittedWhenUnpaired)
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.slots.push_back({
        1u,
        Graphics::GraphicsBindingType::Sampler,
        Graphics::kGraphicsShaderStageFragment,
        false,
        Graphics::kInvalidGraphicsBindingSlot,
    });

    const auto result = Graphics::ValidateGraphicsBindingSet(
        layout,
        {},
        nullptr,
        nullptr);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.code, Graphics::GraphicsBindingValidationCode::None);
}

TEST(GraphicsBindingValidation, InvalidHandleRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back({
        0u,
        Graphics::GraphicsResourceKind::Texture,
        {},
    });

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::InvalidHandle);
    EXPECT_EQ(result.slot, 0u);
    EXPECT_EQ(result.expectedKind, Graphics::GraphicsResourceKind::Texture);
}

TEST(GraphicsBindingValidation, StaleHandleRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());
    backend.DestroyTexture(texture);

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::StaleHandle);
    EXPECT_EQ(result.slot, 0u);
    EXPECT_EQ(result.expectedKind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(result.actualKind, Graphics::GraphicsResourceKind::Invalid);
}

TEST(GraphicsBindingValidation, WrongResourceKindRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back({
        0u,
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer),
    });

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::WrongResourceKind);
    EXPECT_EQ(result.expectedKind, Graphics::GraphicsResourceKind::Texture);
    EXPECT_EQ(result.actualKind, Graphics::GraphicsResourceKind::Buffer);
}

TEST(GraphicsBindingValidation, TextureWithoutRequiredSamplerRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::MissingPairedSampler);
    EXPECT_EQ(result.expectedKind, Graphics::GraphicsResourceKind::Sampler);
}

TEST(GraphicsBindingValidation, WrongPairedSamplerKindRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(buffer.IsValid());

    Graphics::GraphicsBindingSetDesc bindingSet{};
    bindingSet.resources.push_back(MakeTextureBinding(texture));
    bindingSet.resources.push_back({
        1u,
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer),
    });

    const auto result = Graphics::ValidateGraphicsBindingSet(
        MakeTextureSamplerLayout(),
        bindingSet,
        QueryNullBackendResource,
        &backend);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::WrongResourceKind);
    EXPECT_EQ(result.slot, 1u);
    EXPECT_EQ(result.expectedKind, Graphics::GraphicsResourceKind::Sampler);
    EXPECT_EQ(result.actualKind, Graphics::GraphicsResourceKind::Buffer);
}
