/// @file RuntimeAssetCatalogTests.cpp
/// @brief Focused tests for the Runtime read-only asset catalog facade.

#include <SagaRuntime/RuntimeAssetBootstrap.hpp>
#include <SagaRuntime/RuntimeAssetCatalog.hpp>

#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

namespace Resources = SagaEngine::Resources;

using SagaRuntime::RuntimeAssetBootstrap;
using SagaRuntime::RuntimeAssetBootstrapOptions;
using SagaRuntime::RuntimeAssetBootstrapResult;
using SagaRuntime::RuntimeAssetCatalog;

constexpr const char* kRuntimeSmokeAssetKey = "texture.runtime_smoke.albedo";
constexpr Resources::AssetId kRuntimeSmokeAssetId = 5001;

[[nodiscard]] std::filesystem::path FixtureRoot()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" / "Fixtures" /
           "Packages" / "RuntimeSmoke";
}

[[nodiscard]] std::filesystem::path FixturePackageManifestPath()
{
    return FixtureRoot() / "package_manifest.client.json";
}

[[nodiscard]] RuntimeAssetBootstrapOptions OptionsFor(
    const std::filesystem::path& packageManifestPath)
{
    RuntimeAssetBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.expectedRuntimeCompatibilityVersion = "0.0.9";
    options.validateReferencedManifestFiles = true;
    options.validateAssetFiles = true;
    return options;
}

void BootstrapRuntimeSmoke(Resources::AssetRegistry& registry)
{
    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(FixturePackageManifestPath()));

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.registeredAssetCount, 1u);
}

} // namespace

static_assert(std::is_const_v<std::remove_pointer_t<decltype(
                  std::declval<const RuntimeAssetCatalog&>().TryGetById(
                      kRuntimeSmokeAssetId))>>,
              "RuntimeAssetCatalog exposes const registry entries only.");

TEST(RuntimeAssetCatalogTests, EmptyCatalogReportsEmptyAndZeroCount)
{
    Resources::AssetRegistry registry;
    const RuntimeAssetCatalog catalog(registry);

    EXPECT_TRUE(catalog.IsEmpty());
    EXPECT_EQ(catalog.GetAssetCount(), 0u);
    EXPECT_FALSE(catalog.Contains(kRuntimeSmokeAssetId));
    EXPECT_FALSE(catalog.Contains(std::string_view{kRuntimeSmokeAssetKey}));
    EXPECT_EQ(catalog.TryGetById(kRuntimeSmokeAssetId), nullptr);
    EXPECT_EQ(catalog.TryGetByKey(kRuntimeSmokeAssetKey), nullptr);
    EXPECT_TRUE(catalog.FindByKind(Resources::AssetKind::Texture).empty());
    EXPECT_TRUE(catalog.GetPrefetchCandidates().empty());
    EXPECT_EQ(catalog.GetTotalDiskSizeBytes(), 0u);
}

TEST(RuntimeAssetCatalogTests, RuntimeSmokeBootstrapIsVisibleThroughCatalog)
{
    Resources::AssetRegistry registry;
    BootstrapRuntimeSmoke(registry);

    const RuntimeAssetCatalog catalog(registry);

    EXPECT_FALSE(catalog.IsEmpty());
    EXPECT_EQ(catalog.GetAssetCount(), 1u);
    EXPECT_TRUE(catalog.Contains(kRuntimeSmokeAssetId));
    EXPECT_TRUE(catalog.Contains(std::string_view{kRuntimeSmokeAssetKey}));

    const Resources::AssetRegistryEntry* byKey =
        catalog.TryGetByKey(kRuntimeSmokeAssetKey);
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, kRuntimeSmokeAssetId);
    EXPECT_EQ(byKey->assetKey, kRuntimeSmokeAssetKey);
    EXPECT_EQ(byKey->kind, Resources::AssetKind::Texture);

    const Resources::AssetRegistryEntry* byId =
        catalog.TryGetById(kRuntimeSmokeAssetId);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId, byKey);

    const std::vector<const Resources::AssetRegistryEntry*> textures =
        catalog.FindByKind(Resources::AssetKind::Texture);
    ASSERT_EQ(textures.size(), 1u);
    EXPECT_EQ(textures.front(), byKey);

    EXPECT_TRUE(catalog.FindByKind(Resources::AssetKind::Mesh).empty());
    EXPECT_EQ(catalog.GetTotalDiskSizeBytes(), registry.TotalDiskSizeBytes());
}

TEST(RuntimeAssetCatalogTests, MissingAssetLookupsReturnFalseAndNull)
{
    Resources::AssetRegistry registry;
    BootstrapRuntimeSmoke(registry);

    const RuntimeAssetCatalog catalog(registry);

    EXPECT_FALSE(catalog.Contains(Resources::kInvalidAssetId));
    EXPECT_FALSE(catalog.Contains(Resources::AssetId{999999}));
    EXPECT_FALSE(catalog.Contains(std::string_view{"texture.runtime_smoke.missing"}));
    EXPECT_EQ(catalog.TryGetById(Resources::AssetId{999999}), nullptr);
    EXPECT_EQ(catalog.TryGetByKey("texture.runtime_smoke.missing"), nullptr);
}

TEST(RuntimeAssetCatalogTests, CatalogObservesCallerOwnedRegistryState)
{
    Resources::AssetRegistry registry;
    const RuntimeAssetCatalog catalog(registry);

    EXPECT_TRUE(catalog.IsEmpty());

    Resources::AssetRegistryEntry entry;
    entry.assetId = Resources::AssetId{9001};
    entry.assetKey = "texture.runtime_catalog.late_insert";
    entry.kind = Resources::AssetKind::Texture;
    entry.sourcePath = "Cooked/runtime_catalog_late_insert.ktx2";
    entry.diskSizeBytes = 128u;
    registry.Insert(std::move(entry));

    EXPECT_FALSE(catalog.IsEmpty());
    EXPECT_EQ(catalog.GetAssetCount(), 1u);
    EXPECT_TRUE(catalog.Contains(Resources::AssetId{9001}));
    EXPECT_TRUE(catalog.Contains("texture.runtime_catalog.late_insert"));
    EXPECT_EQ(catalog.GetTotalDiskSizeBytes(), 128u);
}

TEST(RuntimeAssetCatalogTests, FailedBootstrapLeavesCatalogOnCommittedState)
{
    Resources::AssetRegistry registry;

    Resources::AssetRegistryEntry existing;
    existing.assetId = kRuntimeSmokeAssetId;
    existing.assetKey = kRuntimeSmokeAssetKey;
    existing.kind = Resources::AssetKind::Texture;
    existing.sourcePath = "already_registered.ktx2";
    existing.diskSizeBytes = 64u;
    registry.Insert(std::move(existing));

    const RuntimeAssetCatalog catalog(registry);

    const RuntimeAssetBootstrapResult result =
        RuntimeAssetBootstrap::RegisterPackageAssets(
            registry,
            OptionsFor(FixturePackageManifestPath()));

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(catalog.GetAssetCount(), 1u);

    const Resources::AssetRegistryEntry* committed =
        catalog.TryGetById(kRuntimeSmokeAssetId);
    ASSERT_NE(committed, nullptr);
    EXPECT_EQ(committed->sourcePath, "already_registered.ktx2");
    EXPECT_EQ(committed->diskSizeBytes, 64u);
}
