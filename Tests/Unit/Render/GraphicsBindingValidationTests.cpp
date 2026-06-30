/// @file GraphicsBindingValidationTests.cpp
/// @brief CPU-side graphics binding contract tests.

#include "SagaEngine/Graphics/Backend/GraphicsBackend.h"
#include "SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

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
    return desc;
}

Graphics::BufferDesc MakeBufferDesc() noexcept
{
    Graphics::BufferDesc desc{};
    desc.sizeBytes = 128u;
    desc.usage = Graphics::BufferUsage::Uniform;
    return desc;
}

Graphics::GraphicsHandle ToGraphicsHandle(
    const Graphics::GraphicsHandle& handle) noexcept
{
    return handle;
}

Graphics::GraphicsResourceQueryResult QueryNullBackendResource(
    void* userData,
    Graphics::GraphicsResourceKind kind,
    Graphics::GraphicsHandle handle)
{
    return static_cast<Graphics::NullGraphicsBackend*>(userData)->QueryResource(
        kind,
        handle);
}

Graphics::GraphicsBindingLayoutSlot MakeTextureSlot(
    std::uint32_t slot = 0u,
    Graphics::GraphicsBindingStableId stableId = 100u)
{
    Graphics::GraphicsBindingLayoutSlot entry{};
    entry.slot = slot;
    entry.stableId = stableId;
    entry.type = Graphics::GraphicsBindingType::SampledTexture;
    entry.stages = Graphics::kGraphicsShaderStageFragment;
    entry.arrayCount = 1u;
    entry.frequency = Graphics::GraphicsBindingFrequency::Material;
    entry.required = true;
    return entry;
}

Graphics::GraphicsBindingLayoutSlot MakeSamplerSlot(
    std::uint32_t slot = 1u,
    Graphics::GraphicsBindingStableId stableId = 101u)
{
    Graphics::GraphicsBindingLayoutSlot entry{};
    entry.slot = slot;
    entry.stableId = stableId;
    entry.type = Graphics::GraphicsBindingType::Sampler;
    entry.stages = Graphics::kGraphicsShaderStageFragment;
    entry.arrayCount = 1u;
    entry.frequency = Graphics::GraphicsBindingFrequency::Material;
    entry.required = false;
    return entry;
}

Graphics::GraphicsBindingLayoutSlot MakeBufferSlot(
    std::uint32_t slot = 2u,
    Graphics::GraphicsBindingStableId stableId = 200u)
{
    Graphics::GraphicsBindingLayoutSlot entry{};
    entry.slot = slot;
    entry.stableId = stableId;
    entry.type = Graphics::GraphicsBindingType::ConstantBuffer;
    entry.stages = Graphics::kGraphicsShaderStageVertex |
                   Graphics::kGraphicsShaderStageFragment;
    entry.arrayCount = 1u;
    entry.frequency = Graphics::GraphicsBindingFrequency::Frame;
    entry.required = true;
    return entry;
}

Graphics::GraphicsBindingLayoutDesc MakeTextureSamplerLayout()
{
    Graphics::GraphicsBindingLayoutDesc layout{};
    layout.debugName = "TextureSamplerLayout";
    auto texture = MakeTextureSlot();
    texture.pairedSamplerSlot = 1u;
    layout.slots.push_back(texture);
    layout.slots.push_back(MakeSamplerSlot());
    return layout;
}

Graphics::GraphicsBindingSetDesc MakeTextureSet(
    Graphics::BindingLayoutHandle layout,
    Graphics::TextureHandle texture,
    Graphics::SamplerHandle sampler)
{
    Graphics::GraphicsBindingSetDesc desc{};
    desc.debugName = "TextureSet";
    desc.layout = layout;
    desc.resources.push_back({
        0u,
        100u,
        Graphics::GraphicsResourceKind::Texture,
        ToGraphicsHandle(texture),
    });
    desc.resources.push_back({
        1u,
        101u,
        Graphics::GraphicsResourceKind::Sampler,
        ToGraphicsHandle(sampler),
    });
    return desc;
}

} // namespace

TEST(BindingLayout, DuplicateSlotRejected)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[1].slot = layout.slots[0].slot;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::DuplicateLayoutSlot);
}

TEST(BindingLayout, DuplicateStableIdRejected)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[1].stableId = layout.slots[0].stableId;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::DuplicateStableId);
}

TEST(BindingLayout, ZeroArrayCountRejected)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[0].arrayCount = 0u;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::ZeroArrayCount);
}

TEST(BindingLayout, ArrayCountAboveMaximumRejected)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[0].arrayCount =
        Graphics::kMaxGraphicsBindingArrayCount + 1u;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::ArrayCountOutOfRange);
}

TEST(BindingLayout, StorageBufferRejectedBeforeCompatibilityUse)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[0].type = Graphics::GraphicsBindingType::StorageBuffer;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::UnsupportedBindingType);
}

TEST(BindingLayout, EmptyStageMaskRejected)
{
    auto layout = MakeTextureSamplerLayout();
    layout.slots[0].stages = 0u;

    const auto result = Graphics::ValidateGraphicsBindingLayout(layout);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(
        result.code,
        Graphics::GraphicsBindingValidationCode::EmptyStageMask);
}

TEST(BindingLayout, HashIsDeterministic)
{
    const auto layout = MakeTextureSamplerLayout();

    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(layout),
        Graphics::ComputeGraphicsBindingLayoutKey(layout));
}

TEST(BindingLayout, InputOrderNormalizesDeterministically)
{
    auto a = MakeTextureSamplerLayout();
    auto b = a;
    std::swap(b.slots[0], b.slots[1]);

    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(a),
        Graphics::ComputeGraphicsBindingLayoutKey(b));
}

TEST(BindingLayout, StaleGenerationRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    backend.DestroyBindingLayout(layout);

    EXPECT_FALSE(backend.QueryBindingLayout(layout).live);
}

TEST(BindingLayout, DoubleDestroyIsNoOp)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    backend.DestroyBindingLayout(layout);
    backend.DestroyBindingLayout(layout);

    EXPECT_FALSE(backend.QueryBindingLayout(layout).live);
}

TEST(BindingLayout, ReusedSlotAdvancesGeneration)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    const auto first = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(first.IsValid());
    backend.DestroyBindingLayout(first);

    const auto second = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(second.IsValid());
    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(backend.QueryBindingLayout(first).live);
}

TEST(BindingSet, RequiredBindingMissingRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidBindingSetDesc);
}

TEST(BindingSet, OptionalTextureUsesFallbackPolicy)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    auto layoutDesc = MakeTextureSamplerLayout();
    layoutDesc.slots[0].required = false;
    layoutDesc.slots[0].fallbackPolicy =
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampledTexture;
    const auto layout = backend.CreateBindingLayout(layoutDesc);
    ASSERT_TRUE(layout.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    const auto handle = backend.CreateBindingSet(set);
    ASSERT_TRUE(handle.IsValid());
    EXPECT_EQ(backend.QueryBindingSet(handle).fallbackRequiredCount, 1u);
}

TEST(BindingSet, TypeMismatchRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    set.resources.push_back({
        0u,
        100u,
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer),
    });

    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
}

TEST(BindingSet, ArrayCountMismatchRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    ASSERT_TRUE(texture.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    set.resources.push_back({
        0u,
        100u,
        Graphics::GraphicsResourceKind::Texture,
        ToGraphicsHandle(texture),
        1u,
    });

    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
}

TEST(BindingSet, InvalidBufferRangeRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::GraphicsBindingLayoutDesc layoutDesc{};
    layoutDesc.slots.push_back(MakeBufferSlot());
    const auto layout = backend.CreateBindingLayout(layoutDesc);
    ASSERT_TRUE(layout.IsValid());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    set.resources.push_back({
        2u,
        200u,
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer),
        0u,
        100u,
        40u,
    });

    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
}

TEST(BindingSet, StaleTextureRejectedAtResolve)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    backend.DestroyTexture(texture);

    EXPECT_FALSE(
        backend.CreateBindingSet(MakeTextureSet(layout, texture, sampler))
            .IsValid());
}

TEST(BindingSet, StaleSamplerRejectedAtResolve)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    backend.DestroySampler(sampler);

    EXPECT_FALSE(
        backend.CreateBindingSet(MakeTextureSet(layout, texture, sampler))
            .IsValid());
}

TEST(BindingSet, StaleBufferRejectedAtResolve)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::GraphicsBindingLayoutDesc layoutDesc{};
    layoutDesc.slots.push_back(MakeBufferSlot());
    const auto layout = backend.CreateBindingLayout(layoutDesc);
    ASSERT_TRUE(layout.IsValid());
    const auto buffer = backend.CreateBuffer(MakeBufferDesc());
    ASSERT_TRUE(buffer.IsValid());
    backend.DestroyBuffer(buffer);

    Graphics::GraphicsBindingSetDesc set{};
    set.layout = layout;
    set.resources.push_back({
        2u,
        200u,
        Graphics::GraphicsResourceKind::Buffer,
        ToGraphicsHandle(buffer),
    });
    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
}

TEST(BindingSet, WrongLayoutRejected)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::GraphicsBindingSetDesc set{};
    EXPECT_FALSE(backend.CreateBindingSet(set).IsValid());
}

TEST(BindingSet, DoubleDestroyIsNoOp)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    const auto set = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(set.IsValid());
    backend.DestroyBindingSet(set);
    backend.DestroyBindingSet(set);

    EXPECT_FALSE(backend.QueryBindingSet(set).live);
}

TEST(BindingSet, ReusedSlotRejectsOldGeneration)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    const auto first = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(first.IsValid());
    backend.DestroyBindingSet(first);
    const auto second = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(second.IsValid());

    EXPECT_EQ(second.index, first.index);
    EXPECT_NE(second.generation, first.generation);
    EXPECT_FALSE(backend.QueryBindingSet(first).live);
}

TEST(BindingSet, BindingSetCreationRejectsAlreadyStaleResource)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());
    backend.DestroyTexture(texture);

    EXPECT_FALSE(
        backend.CreateBindingSet(MakeTextureSet(layout, texture, sampler))
            .IsValid());
}

TEST(BindingSet, DestroyingResourceAfterBindingSetCreationDoesNotCorruptRegistry)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    const auto set = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(set.IsValid());
    backend.DestroyTexture(texture);

    EXPECT_TRUE(backend.QueryBindingSet(set).live);
    const auto replacement = backend.CreateTexture(MakeTextureDesc());
    EXPECT_TRUE(replacement.IsValid());
}

TEST(BindingSet, BindingSetQueryRemainsDeterministicAfterDependencyDestroy)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    const auto set = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(set.IsValid());
    const auto before = backend.QueryBindingSet(set);
    backend.DestroyTexture(texture);
    const auto after = backend.QueryBindingSet(set);

    EXPECT_EQ(after.live, before.live);
    EXPECT_EQ(after.layout.index, before.layout.index);
    EXPECT_EQ(after.layout.generation, before.layout.generation);
    EXPECT_EQ(after.compatibilityKey, before.compatibilityKey);
    EXPECT_EQ(after.bindingCount, before.bindingCount);
}

TEST(BindingSet, RuntimeDependencyRevalidationIsDeferredToM5B)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    const auto texture = backend.CreateTexture(MakeTextureDesc());
    const auto sampler = backend.CreateSampler({});
    ASSERT_TRUE(layout.IsValid());
    ASSERT_TRUE(texture.IsValid());
    ASSERT_TRUE(sampler.IsValid());

    const auto set = backend.CreateBindingSet(
        MakeTextureSet(layout, texture, sampler));
    ASSERT_TRUE(set.IsValid());
    backend.DestroyTexture(texture);

    EXPECT_TRUE(backend.QueryBindingSet(set).live);
    EXPECT_FALSE(backend.QueryResource(
                            Graphics::GraphicsResourceKind::Texture,
                            ToGraphicsHandle(texture))
                     .live);
}

TEST(BindingCompatibility, PipelineDerivesCompatibilityKeyFromLiveLayout)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingLayout = layout;
    EXPECT_TRUE(backend.CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend.GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::None);
}

TEST(BindingCompatibility, PipelineRejectsStaleBindingLayout)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());
    backend.DestroyBindingLayout(layout);

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingLayout = layout;
    EXPECT_FALSE(backend.CreatePipeline(pipeline).IsValid());
}

TEST(BindingCompatibility, PipelineRejectsExplicitKeyMismatch)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingLayout = layout;
    pipeline.bindingCompatibilityKey =
        backend.QueryBindingLayout(layout).compatibilityKey + 1u;
    EXPECT_FALSE(backend.CreatePipeline(pipeline).IsValid());
}

TEST(BindingCompatibility, PipelineRejectsSameKeyCanonicalMismatch)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));
    const auto layout = backend.CreateBindingLayout(MakeTextureSamplerLayout());
    ASSERT_TRUE(layout.IsValid());

    auto different = MakeTextureSamplerLayout();
    different.slots[0].frequency = Graphics::GraphicsBindingFrequency::Draw;

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingLayout = layout;
    pipeline.bindingCompatibilityKey =
        backend.QueryBindingLayout(layout).compatibilityKey;
    pipeline.bindingCompatibilityLayout = different;
    EXPECT_FALSE(backend.CreatePipeline(pipeline).IsValid());
}

TEST(BindingCompatibility, LegacyPipelineKeyWithoutLayoutRemainsSupported)
{
    Graphics::NullGraphicsBackend backend;
    ASSERT_TRUE(backend.Initialize(MakeHeadlessDesc(), {}));

    Graphics::PipelineDesc pipeline{};
    pipeline.bindingCompatibilityKey = 42u;
    EXPECT_TRUE(backend.CreatePipeline(pipeline).IsValid());
}

TEST(BindingCompatibility, SameKeyAndSameCanonicalLayoutIsCompatible)
{
    const auto layout = MakeTextureSamplerLayout();

    EXPECT_TRUE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        layout,
        42u));
}

TEST(BindingCompatibility, SameKeyButDifferentCanonicalLayoutIsRejected)
{
    const auto layout = MakeTextureSamplerLayout();
    auto different = layout;
    different.slots[0].slot = 7u;

    EXPECT_FALSE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        different,
        42u));
}

TEST(BindingCompatibility, DifferentKeyRejectsWithoutCanonicalMatch)
{
    const auto layout = MakeTextureSamplerLayout();

    EXPECT_FALSE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        layout,
        43u));
}

TEST(BindingCompatibility, DebugNameDoesNotAffectCanonicalEquality)
{
    auto a = MakeTextureSamplerLayout();
    auto b = a;
    a.debugName = "A";
    b.debugName = "B";

    EXPECT_TRUE(Graphics::AreGraphicsBindingLayoutsCanonicallyEqual(a, b));
}

TEST(BindingCompatibility, DeclarationOrderDoesNotAffectCanonicalEquality)
{
    auto a = MakeTextureSamplerLayout();
    auto b = a;
    std::swap(b.slots[0], b.slots[1]);

    EXPECT_TRUE(Graphics::AreGraphicsBindingLayoutsCanonicallyEqual(a, b));
}

TEST(BindingCompatibility, FrequencyDifferenceBreaksCompatibility)
{
    const auto layout = MakeTextureSamplerLayout();
    auto different = layout;
    different.slots[0].frequency = Graphics::GraphicsBindingFrequency::Draw;

    EXPECT_FALSE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        different,
        42u));
}

TEST(BindingCompatibility, FallbackDifferenceBreaksCompatibility)
{
    const auto layout = MakeTextureSamplerLayout();
    auto different = layout;
    different.slots[0].required = false;
    different.slots[0].fallbackPolicy =
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampledTexture;

    EXPECT_FALSE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        different,
        42u));
}

TEST(BindingCompatibility, ArrayCountDifferenceBreaksCompatibility)
{
    const auto layout = MakeTextureSamplerLayout();
    auto different = layout;
    different.slots[0].arrayCount = 2u;

    EXPECT_FALSE(Graphics::AreGraphicsBindingLayoutsCompatible(
        layout,
        42u,
        different,
        42u));
}

TEST(BindingCompatibility, DebugNameDoesNotChangeLayoutKey)
{
    auto a = MakeTextureSamplerLayout();
    auto b = a;
    a.debugName = "A";
    b.debugName = "B";

    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(a),
        Graphics::ComputeGraphicsBindingLayoutKey(b));
}

TEST(BindingCompatibility, DeclarationOrderDoesNotChangeLayoutKey)
{
    auto a = MakeTextureSamplerLayout();
    auto b = a;
    std::swap(b.slots[0], b.slots[1]);

    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(a),
        Graphics::ComputeGraphicsBindingLayoutKey(b));
}

TEST(BindingCompatibility, FrequencyOrFallbackPolicyChangesLayoutKey)
{
    auto base = MakeTextureSamplerLayout();
    auto frequency = base;
    frequency.slots[0].frequency = Graphics::GraphicsBindingFrequency::Draw;
    auto fallback = base;
    fallback.slots[0].required = false;
    fallback.slots[0].fallbackPolicy =
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampledTexture;

    EXPECT_NE(
        Graphics::ComputeGraphicsBindingLayoutKey(base),
        Graphics::ComputeGraphicsBindingLayoutKey(frequency));
    EXPECT_NE(
        Graphics::ComputeGraphicsBindingLayoutKey(base),
        Graphics::ComputeGraphicsBindingLayoutKey(fallback));
}

TEST(BindingCompatibility, LayoutKeyGoldenCorpusIsStable)
{
    static_assert(std::is_same_v<
                  decltype(Graphics::ComputeGraphicsBindingLayoutKey(
                      Graphics::GraphicsBindingLayoutDesc{})),
                  std::uint64_t>);

    // Committed HEAD had no layout key API; this pins the current process-local
    // compatibility key corpus so future algorithm changes are explicit.
    Graphics::GraphicsBindingLayoutDesc empty{};
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(empty),
        12161962213042174405ull);

    Graphics::GraphicsBindingLayoutDesc constantBuffer{};
    constantBuffer.slots.push_back({
        0u,
        10u,
        Graphics::GraphicsBindingType::ConstantBuffer,
        Graphics::kGraphicsShaderStageVertex |
            Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Frame,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(constantBuffer),
        16039575791027733390ull);

    Graphics::GraphicsBindingLayoutDesc sampledTexture{};
    sampledTexture.slots.push_back({
        1u,
        20u,
        Graphics::GraphicsBindingType::SampledTexture,
        Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Material,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(sampledTexture),
        13047352348963225970ull);

    Graphics::GraphicsBindingLayoutDesc textureSampler{};
    textureSampler.slots.push_back({
        0u,
        30u,
        Graphics::GraphicsBindingType::SampledTexture,
        Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Material,
        true,
        1u,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    textureSampler.slots.push_back({
        1u,
        31u,
        Graphics::GraphicsBindingType::Sampler,
        Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Material,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(textureSampler),
        2069605417192579749ull);

    auto reversedTextureSampler = textureSampler;
    std::swap(reversedTextureSampler.slots[0], reversedTextureSampler.slots[1]);
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(reversedTextureSampler),
        2069605417192579749ull);

    Graphics::GraphicsBindingLayoutDesc optionalFallback{};
    optionalFallback.slots.push_back({
        2u,
        40u,
        Graphics::GraphicsBindingType::SampledTexture,
        Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Material,
        false,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampledTexture,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(optionalFallback),
        8257562531942669389ull);

    Graphics::GraphicsBindingLayoutDesc frequencyStatic{};
    frequencyStatic.slots.push_back({
        3u,
        50u,
        Graphics::GraphicsBindingType::ConstantBuffer,
        Graphics::kGraphicsShaderStageVertex,
        1u,
        Graphics::GraphicsBindingFrequency::Static,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(frequencyStatic),
        10658552810207945813ull);

    auto frequencyDraw = frequencyStatic;
    frequencyDraw.slots[0].frequency = Graphics::GraphicsBindingFrequency::Draw;
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(frequencyDraw),
        7630673859800412918ull);

    Graphics::GraphicsBindingLayoutDesc multiStage{};
    multiStage.slots.push_back({
        4u,
        60u,
        Graphics::GraphicsBindingType::ConstantBuffer,
        Graphics::kGraphicsShaderStageVertex |
            Graphics::kGraphicsShaderStageFragment |
            Graphics::kGraphicsShaderStageCompute,
        1u,
        Graphics::GraphicsBindingFrequency::Pass,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(multiStage),
        6098713546195627454ull);

    Graphics::GraphicsBindingLayoutDesc textureArray{};
    textureArray.slots.push_back({
        5u,
        70u,
        Graphics::GraphicsBindingType::SampledTexture,
        Graphics::kGraphicsShaderStageFragment,
        4u,
        Graphics::GraphicsBindingFrequency::Material,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::None,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(textureArray),
        13823633422342164193ull);

    Graphics::GraphicsBindingLayoutDesc boundary{};
    boundary.slots.push_back({
        0xFFFFFFFFu,
        0xFFFFFFFFFFFFFFFFull,
        Graphics::GraphicsBindingType::Sampler,
        Graphics::kGraphicsShaderStageFragment,
        1u,
        Graphics::GraphicsBindingFrequency::Material,
        true,
        Graphics::kInvalidGraphicsBindingSlot,
        Graphics::GraphicsBindingFallbackPolicy::OptionalSampler,
    });
    EXPECT_EQ(
        Graphics::ComputeGraphicsBindingLayoutKey(boundary),
        5758133379041105435ull);
}
