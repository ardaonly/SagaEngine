/// @file AssetManifestLoaderTests.cpp
/// @brief Unit tests for runtime asset manifest loading and validation.

#include <SagaEngine/Assets/AssetManifestLoader.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

using SagaEngine::Assets::AssetKind;
using SagaEngine::Assets::AssetManifestDiagnostics::DuplicateId;
using SagaEngine::Assets::AssetManifestDiagnostics::FileMissing;
using SagaEngine::Assets::AssetManifestDiagnostics::InvalidField;
using SagaEngine::Assets::AssetManifestDiagnostics::InvalidPath;
using SagaEngine::Assets::AssetManifestDiagnostics::ManifestMissing;
using SagaEngine::Assets::AssetManifestDiagnostics::UnknownKind;
using SagaEngine::Assets::AssetManifestLoader;
using SagaEngine::Assets::AssetManifestLoadOptions;

[[nodiscard]] std::filesystem::path TempPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

[[nodiscard]] std::filesystem::path WriteTempFile(
    const char* name,
    const char* contents)
{
    const auto path = TempPath(name);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
    return path;
}

} // namespace

TEST(AssetManifestLoaderTests, ValidManifestLoadsRuntimeAssetMetadata)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_valid.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    {
      "id": "texture.hero.diffuse",
      "kind": "texture",
      "path": "Assets/Textures/hero_diffuse.ktx2",
      "hash": "sha256-demo",
      "dependencies": ["material.hero"],
      "memoryEstimateBytes": 4096,
      "streamingGroup": "zone.start",
      "platform": "linux",
      "profile": "dev-client"
    }
  ]
})");

    const auto result = AssetManifestLoader::LoadFromFile(path);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.manifest.schemaVersion, 1u);
    ASSERT_EQ(result.manifest.assets.size(), 1u);
    EXPECT_EQ(result.manifest.assets[0].id, "texture.hero.diffuse");
    EXPECT_EQ(result.manifest.assets[0].kind, AssetKind::Texture);
    EXPECT_EQ(result.manifest.assets[0].path, "Assets/Textures/hero_diffuse.ktx2");
    ASSERT_TRUE(result.manifest.assets[0].hash.has_value());
    EXPECT_EQ(*result.manifest.assets[0].hash, "sha256-demo");
    ASSERT_EQ(result.manifest.assets[0].dependencies.size(), 1u);
    EXPECT_EQ(result.manifest.assets[0].dependencies[0], "material.hero");
    ASSERT_TRUE(result.manifest.assets[0].memoryEstimateBytes.has_value());
    EXPECT_EQ(*result.manifest.assets[0].memoryEstimateBytes, 4096u);
}

TEST(AssetManifestLoaderTests, MissingFileReturnsManifestMissing)
{
    const auto result = AssetManifestLoader::LoadFromFile(
        TempPath("saga_asset_manifest_missing.json"));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, ManifestMissing);
}

TEST(AssetManifestLoaderTests, UnknownKindReturnsUnknownKind)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_unknown_kind.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "asset.one", "kind": "source-png", "path": "Assets/one.bin" }
  ]
})");

    const auto result = AssetManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, UnknownKind);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "kind");
}

TEST(AssetManifestLoaderTests, DuplicateIdsReturnDuplicateId)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_duplicate_id.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "asset.one", "kind": "mesh", "path": "Assets/one.mesh" },
    { "id": "asset.one", "kind": "texture", "path": "Assets/one.ktx2" }
  ]
})");

    const auto result = AssetManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, DuplicateId);
}

TEST(AssetManifestLoaderTests, EscapingPathReturnsInvalidPath)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_escaping_path.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "asset.one", "kind": "mesh", "path": "../Assets/one.mesh" }
  ]
})");

    const auto result = AssetManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidPath);
}

TEST(AssetManifestLoaderTests, NonStringDependencyReturnsInvalidField)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_bad_dependency.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    {
      "id": "asset.one",
      "kind": "mesh",
      "path": "Assets/one.mesh",
      "dependencies": [7]
    }
  ]
})");

    const auto result = AssetManifestLoader::LoadFromFile(path);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, InvalidField);
    ASSERT_TRUE(result.errors[0].fieldName.has_value());
    EXPECT_EQ(*result.errors[0].fieldName, "dependencies");
}

TEST(AssetManifestLoaderTests, ValidationReturnsFileMissingWhenAssetFileIsAbsent)
{
    const auto path = WriteTempFile(
        "saga_asset_manifest_missing_asset_file.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "asset.one", "kind": "texture", "path": "Assets/missing.ktx2" }
  ]
})");

    AssetManifestLoadOptions options;
    options.validateAssetFiles = true;
    options.assetBaseDirectory = TempPath("saga_asset_manifest_missing_asset_root");

    const auto result = AssetManifestLoader::LoadFromFile(path, options);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0].diagnosticId, FileMissing);
}

TEST(AssetManifestLoaderTests, ValidationSucceedsWhenAssetFileExists)
{
    const auto root = TempPath("saga_asset_manifest_existing_asset_root");
    const auto assetPath = root / "Assets" / "existing.ktx2";
    std::filesystem::create_directories(assetPath.parent_path());
    std::ofstream assetOutput(assetPath);
    assetOutput << "asset";

    const auto path = WriteTempFile(
        "saga_asset_manifest_existing_asset_file.json",
        R"({
  "schemaVersion": 1,
  "assets": [
    { "id": "asset.one", "kind": "texture", "path": "Assets/existing.ktx2" }
  ]
})");

    AssetManifestLoadOptions options;
    options.validateAssetFiles = true;
    options.assetBaseDirectory = root;

    const auto result = AssetManifestLoader::LoadFromFile(path, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.manifest.assets.size(), 1u);
    EXPECT_EQ(result.manifest.assets[0].id, "asset.one");
}
