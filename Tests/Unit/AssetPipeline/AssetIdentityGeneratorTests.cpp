/// @file AssetIdentityGeneratorTests.cpp
/// @brief Unit tests for deterministic asset identity allocation.

#include <SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{

using SagaAssetPipeline::AssetId;
using SagaAssetPipeline::AssetIdentityGenerationDiagnostics::AssetIdOverflow;
using SagaAssetPipeline::AssetIdentityGenerationDiagnostics::DuplicateAssetId;
using SagaAssetPipeline::AssetIdentityGenerationDiagnostics::DuplicateInputAssetKey;
using SagaAssetPipeline::AssetIdentityGenerationDiagnostics::InvalidAssetId;
using SagaAssetPipeline::AssetIdentityGenerator;
using SagaAssetPipeline::AssetIdentityInput;
using SagaAssetPipeline::AssetIdentityMapping;

[[nodiscard]] std::optional<AssetId> FindAssetId(
    const std::vector<AssetIdentityMapping>& mappings,
    const std::string& assetKey)
{
    for (const auto& mapping : mappings)
    {
        if (mapping.assetKey == assetKey)
        {
            return mapping.assetId;
        }
    }

    return std::nullopt;
}

} // namespace

TEST(AssetIdentityGeneratorTests, ReusesExistingMapping)
{
    const auto result = AssetIdentityGenerator::Generate(
        {AssetIdentityInput{.assetKey = "texture.hero.diffuse"}},
        {AssetIdentityMapping{
            .assetKey = "texture.hero.diffuse",
            .assetId = 1001,
        }});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.mappings.size(), 1u);
    EXPECT_EQ(result.mappings[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.mappings[0].assetId, 1001u);
}

TEST(AssetIdentityGeneratorTests, AllocatesNewIdsAfterMaxPreviousId)
{
    const auto result = AssetIdentityGenerator::Generate(
        {
            AssetIdentityInput{.assetKey = "material.hero"},
            AssetIdentityInput{.assetKey = "texture.hero.diffuse"},
        },
        {
            AssetIdentityMapping{.assetKey = "deleted.high", .assetId = 2000},
            AssetIdentityMapping{.assetKey = "material.hero", .assetId = 9},
        });

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(FindAssetId(result.mappings, "material.hero"), 9u);
    EXPECT_EQ(FindAssetId(result.mappings, "texture.hero.diffuse"), 2001u);
}

TEST(AssetIdentityGeneratorTests, AllocatesNewAssetKeysInLexicographicOrder)
{
    const auto result = AssetIdentityGenerator::Generate(
        {
            AssetIdentityInput{.assetKey = "texture.zeta"},
            AssetIdentityInput{.assetKey = "texture.alpha"},
            AssetIdentityInput{.assetKey = "texture.middle"},
        },
        {});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.mappings.size(), 3u);
    EXPECT_EQ(result.mappings[0].assetKey, "texture.alpha");
    EXPECT_EQ(result.mappings[0].assetId, 1u);
    EXPECT_EQ(result.mappings[1].assetKey, "texture.middle");
    EXPECT_EQ(result.mappings[1].assetId, 2u);
    EXPECT_EQ(result.mappings[2].assetKey, "texture.zeta");
    EXPECT_EQ(result.mappings[2].assetId, 3u);
}

TEST(AssetIdentityGeneratorTests, DeletedPreviousIdsAreNotReused)
{
    const auto result = AssetIdentityGenerator::Generate(
        {AssetIdentityInput{.assetKey = "texture.new"}},
        {AssetIdentityMapping{.assetKey = "texture.deleted", .assetId = 42}});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.mappings.size(), 1u);
    EXPECT_EQ(result.mappings[0].assetKey, "texture.new");
    EXPECT_EQ(result.mappings[0].assetId, 43u);
}

TEST(AssetIdentityGeneratorTests, DuplicateInputAssetKeyFails)
{
    const auto result = AssetIdentityGenerator::Generate(
        {
            AssetIdentityInput{.assetKey = "texture.hero.diffuse"},
            AssetIdentityInput{.assetKey = "texture.hero.diffuse"},
        },
        {});

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateInputAssetKey);
    EXPECT_EQ(result.errors[0].assetKey, "texture.hero.diffuse");
    EXPECT_TRUE(result.mappings.empty());
}

TEST(AssetIdentityGeneratorTests, DuplicatePreviousAssetIdFails)
{
    const auto result = AssetIdentityGenerator::Generate(
        {
            AssetIdentityInput{.assetKey = "material.hero"},
            AssetIdentityInput{.assetKey = "texture.hero.diffuse"},
        },
        {
            AssetIdentityMapping{.assetKey = "material.hero", .assetId = 1001},
            AssetIdentityMapping{.assetKey = "texture.hero.diffuse", .assetId = 1001},
        });

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateAssetId);
    EXPECT_EQ(result.errors[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.errors[0].assetId, 1001u);
    EXPECT_TRUE(result.mappings.empty());
}

TEST(AssetIdentityGeneratorTests, AssetIdZeroFails)
{
    const auto result = AssetIdentityGenerator::Generate(
        {AssetIdentityInput{.assetKey = "texture.hero.diffuse"}},
        {AssetIdentityMapping{
            .assetKey = "texture.hero.diffuse",
            .assetId = SagaAssetPipeline::kInvalidAssetId,
        }});

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidAssetId);
    EXPECT_EQ(result.errors[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.errors[0].assetId, SagaAssetPipeline::kInvalidAssetId);
    EXPECT_TRUE(result.mappings.empty());
}

TEST(AssetIdentityGeneratorTests, OverflowAtUint64MaxFails)
{
    const auto result = AssetIdentityGenerator::Generate(
        {AssetIdentityInput{.assetKey = "texture.new"}},
        {AssetIdentityMapping{
            .assetKey = "texture.existing",
            .assetId = std::numeric_limits<AssetId>::max(),
        }});

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, AssetIdOverflow);
    EXPECT_EQ(result.errors[0].assetKey, "texture.new");
    EXPECT_EQ(result.errors[0].assetId, std::numeric_limits<AssetId>::max());
    EXPECT_TRUE(result.mappings.empty());
}

TEST(AssetIdentityGeneratorTests, RenameIsTreatedAsNewIdentity)
{
    const auto result = AssetIdentityGenerator::Generate(
        {AssetIdentityInput{.assetKey = "texture.hero.renamed"}},
        {AssetIdentityMapping{
            .assetKey = "texture.hero.old",
            .assetId = 100,
        }});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.mappings.size(), 1u);
    EXPECT_EQ(result.mappings[0].assetKey, "texture.hero.renamed");
    EXPECT_EQ(result.mappings[0].assetId, 101u);
    EXPECT_EQ(FindAssetId(result.mappings, "texture.hero.old"), std::nullopt);
}
