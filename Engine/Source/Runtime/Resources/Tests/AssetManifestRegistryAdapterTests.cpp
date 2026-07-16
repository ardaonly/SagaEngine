/// @file AssetManifestRegistryAdapterTests.cpp
/// @brief Unit tests for registering asset manifests into AssetRegistry.

#include <SagaEngine/Assets/AssetManifest.hpp>
#include <SagaEngine/Resources/AssetIdResolver.h>
#include <SagaEngine/Resources/AssetManifestRegistryAdapter.h>
#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace Assets = SagaEngine::Assets;
namespace Resources = SagaEngine::Resources;

using Resources::AssetIdResolverDiagnostics::DuplicateAssetId;
using Resources::AssetManifestRegistryAdapter;
using Resources::AssetManifestRegistryAdapterDiagnostics::DuplicateManifestAssetKey;
using Resources::AssetManifestRegistryAdapterDiagnostics::DuplicateResolvedAssetId;
using Resources::AssetManifestRegistryAdapterDiagnostics::InvalidResolvedAssetId;
using Resources::AssetManifestRegistryAdapterDiagnostics::MissingAssetIdMapping;
using Resources::AssetManifestRegistryAdapterDiagnostics::RegistryAssetIdCollision;
using Resources::AssetManifestRegistryAdapterDiagnostics::RegistryAssetKeyCollision;
using Resources::AssetManifestRegistryAdapterDiagnostics::UnsupportedKind;
using Resources::AssetManifestRegistryAdapterOptions;
using Resources::AssetRegistry;
using Resources::AssetRegistryEntry;
using Resources::AssetKind;
using Resources::StaticAssetIdResolver;

[[nodiscard]] std::filesystem::path TempPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

[[nodiscard]] Assets::AssetManifestEntry MakeManifestAsset(
    std::string id,
    Assets::AssetKind kind,
    std::string path)
{
    Assets::AssetManifestEntry asset;
    asset.id = std::move(id);
    asset.kind = kind;
    asset.path = std::move(path);
    return asset;
}

[[nodiscard]] Assets::AssetManifest MakeManifest(
    std::initializer_list<Assets::AssetManifestEntry> assets)
{
    Assets::AssetManifest manifest;
    manifest.schemaVersion = 1;
    manifest.assets.assign(assets.begin(), assets.end());
    return manifest;
}

[[nodiscard]] AssetManifestRegistryAdapterOptions MakeOptions()
{
    AssetManifestRegistryAdapterOptions options;
    options.assetManifestPath =
        TempPath("saga_adapter_manifest_root") / "Manifests" / "assets.json";
    options.assetBaseDirectory = TempPath("saga_adapter_asset_root");
    return options;
}

[[nodiscard]] StaticAssetIdResolver MakeResolver(
    std::initializer_list<std::pair<std::string, Resources::AssetId>> mappings)
{
    StaticAssetIdResolver resolver;
    for (const auto& mapping : mappings)
    {
        resolver.AddMapping(mapping.first, mapping.second);
    }
    return resolver;
}

[[nodiscard]] AssetRegistryEntry MakeRegistryEntry(
    Resources::AssetId assetId,
    std::string assetKey)
{
    AssetRegistryEntry entry;
    entry.assetId = assetId;
    entry.assetKey = std::move(assetKey);
    entry.kind = AssetKind::Texture;
    entry.sourcePath = "existing.ktx2";
    return entry;
}

} // namespace

TEST(AssetManifestRegistryAdapterTests, ValidManifestRegistersByAssetIdAndAssetKey)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero_diffuse.ktx2"),
        MakeManifestAsset(
            "mesh.hero",
            Assets::AssetKind::Mesh,
            "Meshes/hero.mesh"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
        {"mesh.hero", 1002},
    });
    AssetRegistry registry;
    const AssetManifestRegistryAdapterOptions options = MakeOptions();

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, options);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 2u);
    const AssetRegistryEntry* texture = registry.Find(1001);
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->assetKey, "texture.hero.diffuse");
    EXPECT_EQ(texture->kind, AssetKind::Texture);
    EXPECT_EQ(registry.FindByKey("texture.hero.diffuse"), texture);

    const AssetRegistryEntry* mesh = registry.FindByKey("mesh.hero");
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->assetId, 1002u);
    EXPECT_EQ(mesh->kind, AssetKind::Mesh);
}

TEST(AssetManifestRegistryAdapterTests, RuntimeCookedPathIsResolvedFromBaseDirectory)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero_diffuse.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;
    const AssetManifestRegistryAdapterOptions options = MakeOptions();

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, options);

    ASSERT_TRUE(result.Succeeded());
    const AssetRegistryEntry* entry = registry.Find(1001);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->sourcePath,
              (options.assetBaseDirectory / "Textures/hero_diffuse.ktx2")
                  .lexically_normal()
                  .string());
}

TEST(AssetManifestRegistryAdapterTests, RuntimeCookedPathFallsBackToManifestParent)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero_diffuse.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;
    AssetManifestRegistryAdapterOptions options = MakeOptions();
    options.assetBaseDirectory.clear();

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, options);

    ASSERT_TRUE(result.Succeeded());
    const AssetRegistryEntry* entry = registry.Find(1001);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->sourcePath,
              (options.assetManifestPath.parent_path() /
               "Textures/hero_diffuse.ktx2")
                  .lexically_normal()
                  .string());
}

TEST(AssetManifestRegistryAdapterTests, MemoryEstimateDoesNotPopulateDiskSize)
{
    Assets::AssetManifestEntry asset = MakeManifestAsset(
        "texture.hero.diffuse",
        Assets::AssetKind::Texture,
        "Textures/hero_diffuse.ktx2");
    asset.memoryEstimateBytes = 4096;
    const Assets::AssetManifest manifest = MakeManifest({asset});
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_TRUE(result.Succeeded());
    const AssetRegistryEntry* entry = registry.Find(1001);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->diskSizeBytes, 0u);
}

TEST(AssetManifestRegistryAdapterTests, DuplicateManifestAssetKeyFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset("texture.hero", Assets::AssetKind::Texture, "a.ktx2"),
        MakeManifestAsset("texture.hero", Assets::AssetKind::Texture, "b.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicateManifestAssetKey);
    EXPECT_EQ(result.diagnostics[0].assetKey, "texture.hero");
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(AssetManifestRegistryAdapterTests, DuplicateResolvedAssetIdFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset("texture.hero", Assets::AssetKind::Texture, "a.ktx2"),
        MakeManifestAsset("material.hero", Assets::AssetKind::Material, "m.mat"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
        {"material.hero", 1001},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicateResolvedAssetId);
    EXPECT_EQ(result.diagnostics[0].assetKey, "material.hero");
    EXPECT_EQ(result.diagnostics[0].assetId, 1001u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(AssetManifestRegistryAdapterTests, ExistingRegistryAssetKeyCollisionFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/new.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1002},
    });
    AssetRegistry registry;
    ASSERT_TRUE(registry.TryInsert(
        MakeRegistryEntry(1001, "texture.hero.diffuse"))
            .Succeeded());

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, RegistryAssetKeyCollision);
    EXPECT_EQ(registry.EntryCount(), 1u);
    EXPECT_EQ(registry.Find(1002), nullptr);
}

TEST(AssetManifestRegistryAdapterTests, ExistingRegistryAssetIdCollisionFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/new.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;
    ASSERT_TRUE(registry.TryInsert(
        MakeRegistryEntry(1001, "texture.existing"))
            .Succeeded());

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, RegistryAssetIdCollision);
    EXPECT_EQ(registry.EntryCount(), 1u);
    EXPECT_EQ(registry.FindByKey("texture.hero.diffuse"), nullptr);
}

TEST(AssetManifestRegistryAdapterTests, MissingAssetIdMappingFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero.ktx2"),
    });
    StaticAssetIdResolver resolver;
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, MissingAssetIdMapping);
    EXPECT_EQ(result.diagnostics[0].assetKey, "texture.hero.diffuse");
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(AssetManifestRegistryAdapterTests, InvalidResolvedAssetIdFailsWithoutMutation)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 0},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, InvalidResolvedAssetId);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(AssetManifestRegistryAdapterTests, UnsupportedKindFailsWithoutSilentFallback)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "asset.unsupported",
            static_cast<Assets::AssetKind>(255),
            "unsupported.bin"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"asset.unsupported", 1001},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, UnsupportedKind);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(AssetManifestRegistryAdapterTests, AdapterDoesNotReadAssetFileContents)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/missing_file.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, MakeOptions());

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    EXPECT_NE(registry.Find(1001), nullptr);
}

TEST(AssetManifestRegistryAdapterTests, AdapterDoesNotRequirePackageManifest)
{
    const Assets::AssetManifest manifest = MakeManifest({
        MakeManifestAsset(
            "texture.hero.diffuse",
            Assets::AssetKind::Texture,
            "Textures/hero.ktx2"),
    });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    AssetRegistry registry;
    AssetManifestRegistryAdapterOptions options;
    options.assetManifestPath = TempPath("saga_adapter_assets.json");

    const auto result = AssetManifestRegistryAdapter::RegisterManifestAssets(
        registry, manifest, resolver, options);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
}
