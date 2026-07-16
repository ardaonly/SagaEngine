/// @file AssetIdResolverTests.cpp
/// @brief Unit tests for explicit runtime asset identity resolution.

#include <SagaEngine/Resources/AssetIdResolver.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <vector>

namespace
{

using SagaEngine::Resources::AssetIdResolver;
using SagaEngine::Resources::AssetIdResolverDiagnostics::DuplicateAssetId;
using SagaEngine::Resources::AssetIdResolverDiagnostics::InvalidAssetId;
using SagaEngine::Resources::AssetIdResolverDiagnostics::MissingKey;
using SagaEngine::Resources::AssetKey;
using SagaEngine::Resources::AssetRegistry;
using SagaEngine::Resources::StaticAssetIdResolver;

} // namespace

TEST(AssetIdResolverTests, StaticResolverMapsKnownAssetKey)
{
    StaticAssetIdResolver resolver;
    resolver.AddMapping("texture.hero.diffuse", 1001);

    const std::vector<AssetKey> keys = {"texture.hero.diffuse"};
    const auto result = AssetIdResolver::ResolveAssetKeys(keys, resolver);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.resolutions.size(), 1u);
    EXPECT_EQ(result.resolutions[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.resolutions[0].assetId, 1001u);
}

TEST(AssetIdResolverTests, MissingAssetKeyReturnsDeterministicDiagnostic)
{
    StaticAssetIdResolver resolver;

    const std::vector<AssetKey> keys = {"texture.hero.diffuse"};
    const auto result = AssetIdResolver::ResolveAssetKeys(keys, resolver);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, MissingKey);
    EXPECT_EQ(result.diagnostics[0].assetKey, "texture.hero.diffuse");
    ASSERT_TRUE(result.diagnostics[0].assetIndex.has_value());
    EXPECT_EQ(*result.diagnostics[0].assetIndex, 0u);
}

TEST(AssetIdResolverTests, InvalidResolvedAssetIdReturnsDiagnostic)
{
    StaticAssetIdResolver resolver;
    resolver.AddMapping("texture.hero.diffuse", 0);

    const std::vector<AssetKey> keys = {"texture.hero.diffuse"};
    const auto result = AssetIdResolver::ResolveAssetKeys(keys, resolver);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, InvalidAssetId);
    EXPECT_EQ(result.diagnostics[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.diagnostics[0].assetId, 0u);
}

TEST(AssetIdResolverTests, DuplicateResolvedAssetIdIsDetectedBeforeRegistration)
{
    StaticAssetIdResolver resolver;
    resolver.AddMapping("texture.hero.diffuse", 42);
    resolver.AddMapping("material.hero", 42);

    const std::vector<AssetKey> keys = {
        "texture.hero.diffuse",
        "material.hero",
    };
    const auto result = AssetIdResolver::ResolveAssetKeys(keys, resolver);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicateAssetId);
    EXPECT_EQ(result.diagnostics[0].assetKey, "material.hero");
    EXPECT_EQ(result.diagnostics[0].assetId, 42u);

    AssetRegistry registry;
    if (result.Succeeded())
    {
        FAIL() << "duplicate ids must be rejected before registry mutation";
    }
    EXPECT_EQ(registry.EntryCount(), 0u);
}
