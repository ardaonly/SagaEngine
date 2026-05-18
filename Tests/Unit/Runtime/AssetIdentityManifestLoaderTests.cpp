/// @file AssetIdentityManifestLoaderTests.cpp
/// @brief Unit tests for runtime asset identity manifest loading.

#include <SagaEngine/Resources/AssetIdentityManifest.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace Resources = SagaEngine::Resources;

using Resources::AssetIdentityManifestDiagnostics::DuplicateAssetId;
using Resources::AssetIdentityManifestDiagnostics::DuplicateAssetKey;
using Resources::AssetIdentityManifestDiagnostics::InvalidField;
using Resources::AssetIdentityManifestDiagnostics::ManifestMissing;
using Resources::AssetIdentityManifestDiagnostics::MissingField;
using Resources::AssetIdentityManifestDiagnostics::ParseFailed;
using Resources::AssetIdentityManifestDiagnostics::UnsupportedVersion;
using Resources::AssetIdentityManifestLoader;

[[nodiscard]] std::filesystem::path TempPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

std::filesystem::path WriteTempFile(
    const std::filesystem::path& path,
    const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
    return path;
}

[[nodiscard]] std::string ValidIdentityManifestJson()
{
    return R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.hero.diffuse", "assetId": 1001 },
    { "assetKey": "material.hero", "assetId": 1002 }
  ]
})";
}

} // namespace

TEST(AssetIdentityManifestLoaderTests, ValidManifestLoadsMappings)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_valid.json"),
        ValidIdentityManifestJson());

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifest.schemaVersion, 1u);
    ASSERT_EQ(result.manifest.mappings.size(), 2u);
    EXPECT_EQ(result.manifest.mappings[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(result.manifest.mappings[0].assetId, 1001u);
    EXPECT_EQ(result.manifest.mappings[1].assetKey, "material.hero");
    EXPECT_EQ(result.manifest.mappings[1].assetId, 1002u);
}

TEST(AssetIdentityManifestLoaderTests, MissingFileReturnsManifestMissing)
{
    const auto result = AssetIdentityManifestLoader::LoadFromFile(
        TempPath("saga_asset_identity_missing.json"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ManifestMissing);
}

TEST(AssetIdentityManifestLoaderTests, InvalidJsonReturnsParseFailed)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_invalid_json.json"),
        "{");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ParseFailed);
}

TEST(AssetIdentityManifestLoaderTests, UnsupportedSchemaReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_unsupported_schema.json"),
        R"({ "schemaVersion": 2, "mappings": [] })");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, UnsupportedVersion);
}

TEST(AssetIdentityManifestLoaderTests, MissingMappingsReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_missing_mappings.json"),
        R"({ "schemaVersion": 1 })");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "mappings");
}

TEST(AssetIdentityManifestLoaderTests, MissingAssetKeyReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_missing_key.json"),
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetId": 1001 }
  ]
})");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, MissingField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "assetKey");
}

TEST(AssetIdentityManifestLoaderTests, EmptyAssetKeyReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_empty_key.json"),
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "", "assetId": 1001 }
  ]
})");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "assetKey");
}

TEST(AssetIdentityManifestLoaderTests, InvalidAssetIdReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_zero_id.json"),
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.hero.diffuse", "assetId": 0 }
  ]
})");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "assetId");
}

TEST(AssetIdentityManifestLoaderTests, DuplicateAssetKeyReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_duplicate_key.json"),
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.hero.diffuse", "assetId": 1001 },
    { "assetKey": "texture.hero.diffuse", "assetId": 1002 }
  ]
})");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateAssetKey);
    EXPECT_EQ(result.errors[0].assetKey, "texture.hero.diffuse");
}

TEST(AssetIdentityManifestLoaderTests, DuplicateAssetIdReturnsDiagnostic)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_duplicate_id.json"),
        R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.hero.diffuse", "assetId": 1001 },
    { "assetKey": "material.hero", "assetId": 1001 }
  ]
})");

    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateAssetId);
    EXPECT_EQ(result.errors[0].assetKey, "material.hero");
    EXPECT_EQ(result.errors[0].assetId, 1001u);
}

TEST(AssetIdentityManifestLoaderTests, BuildResolverMapsValidatedMappings)
{
    const auto path = WriteTempFile(
        TempPath("saga_asset_identity_resolver.json"),
        ValidIdentityManifestJson());
    const auto result = AssetIdentityManifestLoader::LoadFromFile(path);
    ASSERT_TRUE(result.Succeeded());

    const auto resolver =
        AssetIdentityManifestLoader::BuildResolver(result.manifest);

    Resources::AssetId assetId = Resources::kInvalidAssetId;
    ASSERT_TRUE(resolver.ResolveAssetId("texture.hero.diffuse", assetId));
    EXPECT_EQ(assetId, 1001u);
}
