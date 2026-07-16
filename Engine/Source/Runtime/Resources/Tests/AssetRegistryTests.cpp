/// @file AssetRegistryTests.cpp
/// @brief Unit tests for runtime asset registry identity metadata.

#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using SagaEngine::Resources::AssetKind;
using SagaEngine::Resources::AssetRegistry;
using SagaEngine::Resources::AssetRegistryDiagnostics::DuplicateAssetId;
using SagaEngine::Resources::AssetRegistryDiagnostics::DuplicateAssetKey;
using SagaEngine::Resources::AssetRegistryEntry;
namespace AssetFlags = SagaEngine::Resources::AssetFlags;

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

[[nodiscard]] AssetRegistryEntry MakeEntry(
    SagaEngine::Resources::AssetId assetId,
    std::string assetKey,
    std::string sourcePath)
{
    AssetRegistryEntry entry;
    entry.assetId = assetId;
    entry.assetKey = std::move(assetKey);
    entry.kind = AssetKind::Texture;
    entry.sourcePath = std::move(sourcePath);
    entry.flags = AssetFlags::kCooked;
    entry.diskSizeBytes = 128;
    return entry;
}

} // namespace

TEST(AssetRegistryTests, AssetKeyCanBeStoredAndFoundByKey)
{
    AssetRegistry registry;
    const auto result = registry.TryInsert(
        MakeEntry(1001, "texture.hero.diffuse", "Textures/hero.ktx2"));

    ASSERT_TRUE(result.Succeeded()) << result.message;
    const AssetRegistryEntry* byId = registry.Find(1001);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, "texture.hero.diffuse");
    EXPECT_EQ(byId->sourcePath, "Textures/hero.ktx2");

    const AssetRegistryEntry* byKey =
        registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, 1001u);
}

TEST(AssetRegistryTests, ExistingNumericLookupBehaviorRemainsUnchanged)
{
    AssetRegistry registry;
    AssetRegistryEntry entry;
    entry.assetId = 77;
    entry.kind = AssetKind::Mesh;
    entry.sourcePath = "Meshes/tree.mesh";

    registry.Insert(std::move(entry));

    const AssetRegistryEntry* byId = registry.Find(77);
    ASSERT_NE(byId, nullptr);
    EXPECT_TRUE(byId->assetKey.empty());
    EXPECT_EQ(byId->kind, AssetKind::Mesh);
    EXPECT_EQ(registry.FindByKey("mesh.tree"), nullptr);
}

TEST(AssetRegistryTests, OldJsonRegistryLoadingRemainsUnchanged)
{
    const auto path = WriteTempFile(
        "saga_asset_registry_old_json.json",
        R"([
  {
    "id": 10485761,
    "kind": "Texture",
    "path": "textures/hero.ktx2",
    "flags": 20,
    "sizeBytes": 2458624,
    "lodCount": 1
  }
])");

    AssetRegistry registry;
    ASSERT_TRUE(registry.LoadFromJson(path.string()));

    const AssetRegistryEntry* entry = registry.Find(10485761);
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->assetKey.empty());
    EXPECT_EQ(entry->kind, AssetKind::Texture);
    EXPECT_EQ(entry->sourcePath, "textures/hero.ktx2");
    EXPECT_EQ(entry->flags, 20u);
    EXPECT_EQ(entry->diskSizeBytes, 2458624u);
    EXPECT_EQ(entry->lodCount, 1u);
}

TEST(AssetRegistryTests, TryInsertRejectsDuplicateAssetIdWithoutMutation)
{
    AssetRegistry registry;
    ASSERT_TRUE(registry.TryInsert(
        MakeEntry(1001, "texture.hero.diffuse", "Textures/hero.ktx2"))
            .Succeeded());

    const auto duplicate = registry.TryInsert(
        MakeEntry(1001, "texture.hero.normal", "Textures/normal.ktx2"));

    ASSERT_FALSE(duplicate.Succeeded());
    EXPECT_EQ(duplicate.diagnosticId, DuplicateAssetId);
    EXPECT_EQ(registry.EntryCount(), 1u);
    EXPECT_EQ(registry.FindByKey("texture.hero.normal"), nullptr);
    EXPECT_NE(registry.FindByKey("texture.hero.diffuse"), nullptr);
}

TEST(AssetRegistryTests, TryInsertRejectsDuplicateAssetKeyWithoutMutation)
{
    AssetRegistry registry;
    ASSERT_TRUE(registry.TryInsert(
        MakeEntry(1001, "texture.hero.diffuse", "Textures/hero.ktx2"))
            .Succeeded());

    const auto duplicate = registry.TryInsert(
        MakeEntry(1002, "texture.hero.diffuse", "Textures/duplicate.ktx2"));

    ASSERT_FALSE(duplicate.Succeeded());
    EXPECT_EQ(duplicate.diagnosticId, DuplicateAssetKey);
    EXPECT_EQ(registry.EntryCount(), 1u);
    EXPECT_EQ(registry.Find(1002), nullptr);
    const AssetRegistryEntry* original =
        registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(original, nullptr);
    EXPECT_EQ(original->assetId, 1001u);
}

TEST(AssetRegistryTests, TryInsertAllRejectsBatchDuplicatesWithoutMutation)
{
    AssetRegistry registry;
    std::vector<AssetRegistryEntry> entries;
    entries.push_back(
        MakeEntry(1001, "texture.hero.diffuse", "Textures/hero.ktx2"));
    entries.push_back(
        MakeEntry(1001, "texture.hero.normal", "Textures/normal.ktx2"));
    entries.push_back(
        MakeEntry(1002, "texture.hero.diffuse", "Textures/duplicate.ktx2"));

    const auto result = registry.TryInsertAll(std::move(entries));

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.insertedCount, 0u);
    EXPECT_EQ(result.diagnostics.size(), 2u);
    EXPECT_EQ(registry.EntryCount(), 0u);
    EXPECT_EQ(registry.Find(1001), nullptr);
    EXPECT_EQ(registry.FindByKey("texture.hero.diffuse"), nullptr);
}
