/// @file SandboxImGuiOverlayTokenTests.cpp
/// @brief Regression coverage for opaque ImGui overlay texture token packing.

#include "SagaSandbox/UI/SandboxImGuiOverlayToken.h"

#define private public
#include "SagaSandbox/UI/SandboxImGuiOverlayAdapter.h"
#undef private

#include <gtest/gtest.h>

#include <cstdint>

namespace
{

using namespace SagaSandbox::Detail;

[[nodiscard]] SagaEngine::Render::Backend::RenderOverlayTextureHandle
OverlayHandle(std::uint32_t index, std::uint32_t generation = 1u) noexcept
{
    return {index, generation};
}

TEST(SandboxImGuiOverlayToken, PackedTokenRoundTripsFields)
{
    constexpr auto token = PackOverlayToken(0x1234u, 0x56u, 0x78u);

    ASSERT_NE(token, 0u);
    EXPECT_EQ(OverlayTokenMagic(token),
              sizeof(std::uintptr_t) >= 8u ? 0x1234u : 0x34u);
    EXPECT_EQ(OverlayTokenGeneration(token), 0x56u);
    EXPECT_EQ(OverlayTokenId(token), 0x78u);
}

TEST(SandboxImGuiOverlayToken, RejectsZeroFieldsAndOutOfRangeId)
{
    EXPECT_EQ(PackOverlayToken(0u, 1u, 1u), 0u);
    EXPECT_EQ(PackOverlayToken(1u, 0u, 1u), 0u);
    EXPECT_EQ(PackOverlayToken(1u, 1u, 0u), 0u);
    EXPECT_EQ(PackOverlayToken(1u, 1u, OverlayTokenMaxId() + 1u), 0u);
}

TEST(SandboxImGuiOverlayToken, PackedTokenIsNotNullOrSmallInteger)
{
    constexpr auto token = PackOverlayToken(1u, 1u, 1u);

    ASSERT_NE(token, 0u);
    EXPECT_GT(token, static_cast<std::uintptr_t>(OverlayTokenMaxId()));
}

TEST(SandboxImGuiOverlayToken, IntegerTextureIdRoundTrips)
{
    const auto token = PackOverlayToken(2u, 3u, 4u);

    const auto texture = OverlayTokenToImTextureID<std::uintptr_t>(token);

    EXPECT_EQ(ImTextureIDToOverlayToken(texture), token);
}

TEST(SandboxImGuiOverlayToken, PointerTextureIdRoundTripsWithoutDereference)
{
    const auto token = PackOverlayToken(5u, 6u, 7u);

    void* texture = OverlayTokenToImTextureID<void*>(token);

    EXPECT_EQ(ImTextureIDToOverlayToken(texture), token);
}

TEST(SandboxImGuiOverlayAdapterToken, SameTextureReturnsStableToken)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;
    const auto token = adapter.CreateToken(OverlayHandle(1u, 7u));

    EXPECT_NE(token, 0u);
    EXPECT_EQ(adapter.CreateToken(OverlayHandle(1u, 7u)), token);
    ASSERT_NE(adapter.ResolveToken(token), nullptr);
    EXPECT_EQ(adapter.ResolveToken(token)->handle, OverlayHandle(1u, 7u));
}

TEST(SandboxImGuiOverlayAdapterToken, DestroyTokenInvalidatesLookup)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;
    const auto token = adapter.CreateToken(OverlayHandle(2u));

    adapter.DestroyToken(token);

    EXPECT_EQ(adapter.ResolveToken(token), nullptr);
}

TEST(SandboxImGuiOverlayAdapterToken, AdapterShutdownInvalidatesAllTokens)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;
    const auto first = adapter.CreateToken(OverlayHandle(3u));
    const auto second = adapter.CreateToken(OverlayHandle(4u));

    adapter.InvalidateAllTokens();

    EXPECT_EQ(adapter.ResolveToken(first), nullptr);
    EXPECT_EQ(adapter.ResolveToken(second), nullptr);
}

TEST(SandboxImGuiOverlayAdapterToken, ReusedTokenSlotRejectsOldGeneration)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;
    const auto oldToken = adapter.CreateToken(OverlayHandle(5u));
    adapter.DestroyToken(oldToken);

    const auto newToken = adapter.CreateToken(OverlayHandle(6u));

    EXPECT_NE(newToken, 0u);
    EXPECT_EQ(OverlayTokenId(newToken), OverlayTokenId(oldToken));
    EXPECT_NE(OverlayTokenGeneration(newToken), OverlayTokenGeneration(oldToken));
    EXPECT_EQ(adapter.ResolveToken(oldToken), nullptr);
    ASSERT_NE(adapter.ResolveToken(newToken), nullptr);
    EXPECT_EQ(adapter.ResolveToken(newToken)->handle, OverlayHandle(6u));
}

TEST(SandboxImGuiOverlayAdapterToken, FakeTokenIsRejected)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;
    const auto fake = PackOverlayToken(adapter.m_instanceMagic, 1u, 42u);

    EXPECT_EQ(adapter.ResolveToken(fake), nullptr);
}

TEST(SandboxImGuiOverlayAdapterToken, TokenFromDifferentAdapterIsRejected)
{
    SagaSandbox::SandboxImGuiOverlayAdapter first;
    SagaSandbox::SandboxImGuiOverlayAdapter second;
    const auto token = first.CreateToken(OverlayHandle(7u));

    EXPECT_NE(token, 0u);
    EXPECT_EQ(second.ResolveToken(token), nullptr);
}

TEST(SandboxImGuiOverlayAdapterToken, NullTokenIsRejected)
{
    SagaSandbox::SandboxImGuiOverlayAdapter adapter;

    EXPECT_EQ(adapter.ResolveToken(0u), nullptr);
}

} // namespace
