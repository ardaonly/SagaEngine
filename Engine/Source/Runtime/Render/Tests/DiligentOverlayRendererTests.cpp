/// @file DiligentOverlayRendererTests.cpp
/// @brief CPU-only tests for private overlay translation and affinity rules.

#include "SagaEngine/Render/Backend/Diligent/DiligentOverlayTextureRegistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Graphics::TextureHandle;
using SagaEngine::Render::Backend::DiligentOverlayTextureRegistry;
using SagaEngine::Render::Backend::RenderOverlayTextureHandle;
using SagaEngine::Render::Backend::RenderOverlayDrawCommand;
using SagaEngine::Render::Backend::RenderOverlayDrawList;
using SagaEngine::Render::Backend::RenderOverlayFrame;
using SagaEngine::Render::Backend::RenderOverlayVertex;
using SagaEngine::Render::Backend::ValidateOverlayFrameForTests;

[[nodiscard]] TextureHandle NativeTexture(
    std::uint32_t index,
    std::uint32_t generation = 1u) noexcept
{
    TextureHandle handle{};
    handle.index = index;
    handle.generation = generation;
    return handle;
}

[[nodiscard]] RenderOverlayFrame ValidFrame()
{
    RenderOverlayDrawList list{};
    list.vertices = {
        {{0.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu},
        {{1.0f, 0.0f}, {1.0f, 0.0f}, 0xFFFFFFFFu},
        {{1.0f, 1.0f}, {1.0f, 1.0f}, 0xFFFFFFFFu},
    };
    list.indices = {0u, 1u, 2u};
    RenderOverlayDrawCommand cmd{};
    cmd.elementCount = 3u;
    list.commands.push_back(cmd);

    RenderOverlayFrame frame{};
    frame.displaySize[0] = 64.0f;
    frame.displaySize[1] = 64.0f;
    frame.drawLists.push_back(std::move(list));
    return frame;
}

} // namespace

TEST(DiligentOverlayTextureRegistry, DoubleDestroyIsIdempotent)
{
    DiligentOverlayTextureRegistry registry;
    const auto handle = registry.Create(NativeTexture(10u));

    const auto first = registry.Destroy(handle);
    const auto second = registry.Destroy(handle);

    EXPECT_TRUE(first.destroyed);
    EXPECT_EQ(first.nativeTexture.index, 10u);
    EXPECT_FALSE(second.destroyed);
    EXPECT_FALSE(registry.Resolve(handle).IsValid());
    EXPECT_EQ(registry.FreeCountForTests(), 1u);
}

TEST(DiligentOverlayTextureRegistry, WrongGenerationDestroyDoesNotCorruptSlot)
{
    DiligentOverlayTextureRegistry registry;
    const auto handle = registry.Create(NativeTexture(11u));
    const RenderOverlayTextureHandle wrongGeneration{
        handle.index,
        handle.generation + 1u,
    };

    const auto result = registry.Destroy(wrongGeneration);

    EXPECT_FALSE(result.destroyed);
    EXPECT_TRUE(registry.Resolve(handle).IsValid());
    EXPECT_EQ(registry.FreeCountForTests(), 0u);
}

TEST(DiligentOverlayTextureRegistry, DestroyImmediatelyRejectsStaleHandle)
{
    DiligentOverlayTextureRegistry registry;
    const auto stale = registry.Create(NativeTexture(12u));

    const auto result = registry.Destroy(stale);

    ASSERT_TRUE(result.destroyed);
    EXPECT_FALSE(registry.Resolve(stale).IsValid());
}

TEST(DiligentOverlayTextureRegistry, TranslationSlotReuseAdvancesGeneration)
{
    DiligentOverlayTextureRegistry registry;
    const auto stale = registry.Create(NativeTexture(13u));
    ASSERT_TRUE(registry.Destroy(stale).destroyed);

    const auto replacement = registry.Create(NativeTexture(14u));

    EXPECT_EQ(replacement.index, stale.index);
    EXPECT_NE(replacement.generation, stale.generation);
    EXPECT_FALSE(registry.Resolve(stale).IsValid());
    EXPECT_EQ(registry.Resolve(replacement).index, 14u);
}

TEST(DiligentOverlayTextureRegistry, DestroyReturnsNativeTextureForDeferredOwnerRetirement)
{
    DiligentOverlayTextureRegistry registry;
    const auto handle = registry.Create(NativeTexture(15u, 9u));

    const auto result = registry.Destroy(handle);

    EXPECT_TRUE(result.destroyed);
    EXPECT_EQ(result.nativeTexture.index, 15u);
    EXPECT_EQ(result.nativeTexture.generation, 9u);
    EXPECT_FALSE(registry.Resolve(handle).IsValid());
}

TEST(DiligentOverlayTextureRegistry, GenerationWrapSkipsZero)
{
    EXPECT_EQ(DiligentOverlayTextureRegistry::AdvanceGeneration(0u), 1u);
    EXPECT_EQ(DiligentOverlayTextureRegistry::AdvanceGeneration(1u), 2u);
    EXPECT_EQ(
        DiligentOverlayTextureRegistry::AdvanceGeneration(
            std::numeric_limits<std::uint32_t>::max()),
        1u);
}

TEST(DiligentOverlayTextureRegistry, OverlayTextureHandleStress)
{
    DiligentOverlayTextureRegistry registry;
    std::vector<RenderOverlayTextureHandle> handles;
    handles.reserve(1000u);

    for (std::uint32_t i = 0u; i < 1000u; ++i)
    {
        auto handle = registry.Create(NativeTexture(1000u + i));
        ASSERT_TRUE(handle);
        handles.push_back(handle);
        const auto result = registry.Destroy(handle);
        ASSERT_TRUE(result.destroyed);
        ASSERT_FALSE(registry.Resolve(handle).IsValid());
    }

    std::vector<std::uint32_t> order(handles.size());
    for (std::uint32_t i = 0u; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::mt19937 rng{0x5A6A2026u};
    std::shuffle(order.begin(), order.end(), rng);

    std::vector<RenderOverlayTextureHandle> current;
    current.reserve(order.size());
    for (const auto i : order)
    {
        auto handle = registry.Create(NativeTexture(3000u + i));
        ASSERT_TRUE(handle);
        EXPECT_EQ(handle.index, 1u);
        EXPECT_FALSE(registry.Resolve(handles[i]).IsValid());
        EXPECT_TRUE(registry.Resolve(handle).IsValid());
        current.push_back(handle);
        ASSERT_TRUE(registry.Destroy(handle).destroyed);
        EXPECT_FALSE(registry.Destroy(handle).destroyed);
    }

    for (const auto handle : handles)
    {
        EXPECT_FALSE(registry.Resolve(handle).IsValid());
    }
    for (const auto handle : current)
    {
        EXPECT_FALSE(registry.Resolve(handle).IsValid());
    }

    const auto live = registry.Create(NativeTexture(9000u));
    ASSERT_TRUE(live);
    const auto retired = registry.DestroyAll();
    EXPECT_EQ(retired.size(), 1u);
    EXPECT_FALSE(registry.Resolve(live).IsValid());

    const auto nextSession = registry.Create(NativeTexture(9001u));
    ASSERT_TRUE(nextSession);
    EXPECT_FALSE(registry.Resolve(live).IsValid());
    EXPECT_TRUE(registry.Resolve(nextSession).IsValid());
}

TEST(DiligentOverlayValidation, MaximumAllowedVertexAndIndexBudgetIsAccepted)
{
    RenderOverlayDrawList list{};
    list.vertices.resize(1'000'000u);
    list.indices.resize(3'000'000u, 0u);

    RenderOverlayFrame frame{};
    frame.displaySize[0] = 64.0f;
    frame.displaySize[1] = 64.0f;
    frame.drawLists.push_back(std::move(list));

    EXPECT_TRUE(ValidateOverlayFrameForTests(frame));
}

TEST(DiligentOverlayValidation, OneOverBudgetIsRejected)
{
    auto frame = ValidFrame();
    frame.drawLists.front().vertices.resize(1'000'001u);

    EXPECT_FALSE(ValidateOverlayFrameForTests(frame));
}

TEST(DiligentOverlayValidation, RepeatedCommandsCannotExceedSubmittedIndexBudget)
{
    RenderOverlayDrawList list{};
    list.vertices.resize(1u);
    list.indices.resize(3'000'000u, 0u);

    RenderOverlayDrawCommand first{};
    first.elementCount = 2'000'000u;
    RenderOverlayDrawCommand second{};
    second.elementCount = 2'000'000u;
    list.commands = {first, second};

    RenderOverlayFrame frame{};
    frame.displaySize[0] = 64.0f;
    frame.displaySize[1] = 64.0f;
    frame.drawLists.push_back(std::move(list));

    EXPECT_FALSE(ValidateOverlayFrameForTests(frame));
}

TEST(DiligentOverlayValidation, Uint32LimitCommandOffsetsAreRejectedBeforeAccess)
{
    auto frame = ValidFrame();
    frame.drawLists.front().commands.front().indexOffset =
        std::numeric_limits<std::uint32_t>::max();

    EXPECT_FALSE(ValidateOverlayFrameForTests(frame));

    frame = ValidFrame();
    frame.drawLists.front().commands.front().vertexOffset =
        std::numeric_limits<std::uint32_t>::max();

    EXPECT_FALSE(ValidateOverlayFrameForTests(frame));
}

TEST(DiligentOverlayValidation, InvalidCommandDoesNotPoisonNextFrame)
{
    auto invalid = ValidFrame();
    invalid.drawLists.front().commands.front().elementCount = 4u;
    EXPECT_FALSE(ValidateOverlayFrameForTests(invalid));

    EXPECT_TRUE(ValidateOverlayFrameForTests(ValidFrame()));
}
