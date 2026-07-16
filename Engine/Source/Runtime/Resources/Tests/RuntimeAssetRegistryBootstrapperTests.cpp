/// @file RuntimeAssetRegistryBootstrapperTests.cpp
/// @brief Unit tests for package asset registry bootstrap.

#include <SagaEngine/Packages/PackageManifest.hpp>
#include <SagaEngine/Resources/AssetIdentityManifest.h>
#include <SagaEngine/Resources/AssetIdResolver.h>
#include <SagaEngine/Resources/AssetRegistry.h>
#include <SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace
{

namespace Packages = SagaEngine::Packages;
namespace Resources = SagaEngine::Resources;

using Resources::RuntimeAssetRegistryBootstrapDiagnostics::DuplicatePackageAssetId;
using Resources::RuntimeAssetRegistryBootstrapDiagnostics::DuplicatePackageAssetKey;
using Resources::RuntimeAssetRegistryBootstrapDiagnostics::MissingAssetIdentityManifest;
using Resources::RuntimeAssetRegistryBootstrapDiagnostics::RegistryAssetIdCollision;
using Resources::RuntimeAssetRegistryBootstrapDiagnostics::RegistryAssetKeyCollision;
using Resources::RuntimeAssetRegistryBootstrapOptions;
using Resources::RuntimeAssetRegistryBootstrapper;
using Resources::StaticAssetIdResolver;

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << contents;
}

[[nodiscard]] std::string AssetManifestJson(
    const char* assetKey,
    const char* kind,
    const char* path)
{
    return std::string(R"({
  "schemaVersion": 1,
  "assets": [
    { "id": ")") + assetKey + R"(", "kind": ")" + kind +
           R"(", "path": ")" + path + R"(" }
  ]
})";
}

[[nodiscard]] std::string AssetIdentityManifestJson(
    const char* assetKey,
    Resources::AssetId assetId)
{
    return std::string(R"({
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": ")") + assetKey + R"(", "assetId": )" +
           std::to_string(assetId) + R"( }
  ]
})";
}

[[nodiscard]] std::string AssetIdentityManifestJson(
    std::initializer_list<std::pair<const char*, Resources::AssetId>> mappings)
{
    std::string json = R"({
  "schemaVersion": 1,
  "mappings": [
)";
    bool first = true;
    for (const auto& mapping : mappings)
    {
        if (!first)
        {
            json += ",\n";
        }

        json += std::string(R"(    { "assetKey": ")") + mapping.first +
                R"(", "assetId": )" + std::to_string(mapping.second) +
                R"( })";
        first = false;
    }

    json += R"(
  ]
})";
    return json;
}

[[nodiscard]] Packages::PackageManifest MakePackage(
    Packages::PackageKind kind,
    std::initializer_list<Packages::PackageManifestRef> assetRefs)
{
    Packages::PackageManifest manifest;
    manifest.schemaVersion = 1;
    manifest.packageId = "test.package";
    manifest.packageKind = kind;
    manifest.buildProfile = "dev-client";
    manifest.targetPlatform = "linux";
    manifest.runtimeCompatibilityVersion = "0.0.8";
    manifest.assetManifests.assign(assetRefs.begin(), assetRefs.end());
    return manifest;
}

[[nodiscard]] Packages::PackageManifestRef MakeRef(
    std::string id,
    std::string path)
{
    Packages::PackageManifestRef ref;
    ref.id = std::move(id);
    ref.path = std::move(path);
    return ref;
}

[[nodiscard]] RuntimeAssetRegistryBootstrapOptions MakeOptions(
    const std::filesystem::path& packageManifestPath)
{
    RuntimeAssetRegistryBootstrapOptions options;
    options.packageManifestPath = packageManifestPath;
    options.validateAssetFiles = true;
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

[[nodiscard]] Resources::AssetRegistryEntry MakeRegistryEntry(
    Resources::AssetId assetId,
    std::string assetKey)
{
    Resources::AssetRegistryEntry entry;
    entry.assetId = assetId;
    entry.assetKey = std::move(assetKey);
    entry.kind = Resources::AssetKind::Texture;
    entry.sourcePath = "existing.ktx2";
    return entry;
}

} // namespace

TEST(RuntimeAssetRegistryBootstrapperTests, ValidClientPackageRegistersAssets)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_client");
    const auto packagePath = root / "package.json";
    const auto manifestPath = root / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero.diffuse",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero.diffuse", 1001},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    const Resources::AssetRegistryEntry* entry = registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->assetId, 1001u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, PackageIdentityManifestRegistersAssets)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_identity");
    const auto packagePath = root / "package.json";
    const auto manifestPath = root / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero.diffuse",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("texture.hero.diffuse", 1001));
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/asset_identity.json";
    Resources::AssetRegistry registry;

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry, package, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    const Resources::AssetRegistryEntry* entry =
        registry.FindByKey("texture.hero.diffuse");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->assetId, 1001u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, PackageIdentityManifestValidationDoesNotRegisterAssets)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_identity_validate");
    const auto packagePath = root / "package.json";
    const auto manifestPath = root / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero.diffuse",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson({
                  {"texture.hero.diffuse", 1001},
                  {"texture.unused.extra", 1002},
              }));
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/asset_identity.json";

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                package, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidationMissingIdentityFieldFailsDeterministically)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_validate_no_identity");
    const auto packagePath = root / "package.json";
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              MissingAssetIdentityManifest);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidationMissingIdentityFileFailsDeterministically)
{
    const auto root =
        TempRoot("saga_runtime_registry_bootstrap_validate_missing_identity");
    const auto packagePath = root / "package.json";
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/missing_identity.json";

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              Resources::AssetIdentityManifestDiagnostics::ManifestMissing);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidationMissingIdentityMappingFails)
{
    const auto root =
        TempRoot("saga_runtime_registry_bootstrap_validate_missing_mapping");
    const auto packagePath = root / "package.json";
    const auto manifestPath = root / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero.diffuse",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("material.hero", 1001));
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/asset_identity.json";

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              Resources::AssetManifestRegistryAdapterDiagnostics::
                  MissingAssetIdMapping);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidationDuplicateAssetKeyAcrossManifestsFails)
{
    const auto root =
        TempRoot("saga_runtime_registry_bootstrap_validate_duplicate_key");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "a.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/a.ktx2"));
    WriteFile(root / "Manifests" / "b.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/b.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "a.ktx2", "asset");
    WriteFile(root / "Manifests" / "Cooked" / "b.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("texture.hero", 1001));
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {
            MakeRef("assets.a", "Manifests/a.json"),
            MakeRef("assets.b", "Manifests/b.json"),
        });
    package.assetIdentityManifest = "Manifests/asset_identity.json";

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            ValidatePackageAssetsFromPackageIdentityManifest(
                package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicatePackageAssetKey);
}

TEST(RuntimeAssetRegistryBootstrapperTests, MissingIdentityFieldFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_no_identity_field");
    const auto packagePath = root / "package.json";
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    Resources::AssetRegistry registry;

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry, package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              MissingAssetIdentityManifest);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, MissingIdentityFileFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_missing_identity");
    const auto packagePath = root / "package.json";
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/missing_identity.json";
    Resources::AssetRegistry registry;

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry, package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              Resources::AssetIdentityManifestDiagnostics::ManifestMissing);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, InvalidIdentityManifestFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_invalid_identity");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "asset_identity.json", "{");
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/asset_identity.json";
    Resources::AssetRegistry registry;

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry, package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              Resources::AssetIdentityManifestDiagnostics::ParseFailed);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, MissingIdentityMappingFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_missing_mapping");
    const auto packagePath = root / "package.json";
    const auto manifestPath = root / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero.diffuse",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    WriteFile(root / "Manifests" / "asset_identity.json",
              AssetIdentityManifestJson("material.hero", 1001));
    Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    package.assetIdentityManifest = "Manifests/asset_identity.json";
    Resources::AssetRegistry registry;

    const auto result =
        RuntimeAssetRegistryBootstrapper::
            RegisterPackageAssetsFromPackageIdentityManifest(
                registry, package, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId,
              Resources::AssetManifestRegistryAdapterDiagnostics::
                  MissingAssetIdMapping);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidServerPackageRegistersAssets)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_server");
    const auto packagePath = root / "server-package.json";
    const auto manifestPath = root / "Manifests" / "server-assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "mesh.npc.guard",
                                "mesh",
                                "Cooked/guard.mesh"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "guard.mesh", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Server,
        {MakeRef("assets.server", "Manifests/server-assets.json")});
    StaticAssetIdResolver resolver = MakeResolver({
        {"mesh.npc.guard", 2001},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 1u);
    EXPECT_NE(registry.Find(2001), nullptr);
}

TEST(RuntimeAssetRegistryBootstrapperTests, MultipleManifestsRegisterAtomically)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_multi");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "a.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/a.ktx2"));
    WriteFile(root / "Manifests" / "b.json",
              AssetManifestJson("material.hero", "material", "Cooked/b.mat"));
    WriteFile(root / "Manifests" / "Cooked" / "a.ktx2", "asset");
    WriteFile(root / "Manifests" / "Cooked" / "b.mat", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {
            MakeRef("assets.a", "Manifests/a.json"),
            MakeRef("assets.b", "Manifests/b.json"),
        });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
        {"material.hero", 1002},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.registeredAssetCount, 2u);
    EXPECT_EQ(registry.EntryCount(), 2u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, RegisterPackageAssetsStoresResolvedRuntimeCookedPath)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_paths");
    const auto packagePath = root / "Packages" / "package.json";
    const auto manifestPath = root / "Packages" / "Manifests" / "assets.json";
    WriteFile(manifestPath, AssetManifestJson(
                                "texture.hero",
                                "texture",
                                "Cooked/hero.ktx2"));
    WriteFile(manifestPath.parent_path() / "Cooked" / "hero.ktx2", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_TRUE(result.Succeeded());
    const Resources::AssetRegistryEntry* entry = registry.Find(1001);
    ASSERT_NE(entry, nullptr);
    // Current registry behavior stores the resolved cooked/runtime path, not a VFS path.
    EXPECT_EQ(entry->sourcePath,
              (manifestPath.parent_path() / "Cooked" / "hero.ktx2")
                  .lexically_normal()
                  .string());
}

TEST(RuntimeAssetRegistryBootstrapperTests, ValidateAssetFilesFalseAllowsMissingCookedFiles)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_no_file_validation");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/missing.ktx2"));
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
    });
    Resources::AssetRegistry registry;
    RuntimeAssetRegistryBootstrapOptions options = MakeOptions(packagePath);
    options.validateAssetFiles = false;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, options);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(registry.EntryCount(), 1u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, MissingAndInvalidManifestDiagnosticsDoNotMutate)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_bad_manifests");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "invalid.json", "{");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {
            MakeRef("assets.missing", "Manifests/missing.json"),
            MakeRef("assets.invalid", "Manifests/invalid.json"),
        });
    StaticAssetIdResolver resolver;
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.size(), 2u);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, DuplicateAssetKeyAcrossManifestsFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_duplicate_key");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "a.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/a.ktx2"));
    WriteFile(root / "Manifests" / "b.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/b.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "a.ktx2", "asset");
    WriteFile(root / "Manifests" / "Cooked" / "b.ktx2", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {
            MakeRef("assets.a", "Manifests/a.json"),
            MakeRef("assets.b", "Manifests/b.json"),
        });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicatePackageAssetKey);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, DuplicateAssetIdAcrossManifestsFailsWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_duplicate_id");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "a.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/a.ktx2"));
    WriteFile(root / "Manifests" / "b.json",
              AssetManifestJson("material.hero", "material", "Cooked/b.mat"));
    WriteFile(root / "Manifests" / "Cooked" / "a.ktx2", "asset");
    WriteFile(root / "Manifests" / "Cooked" / "b.mat", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {
            MakeRef("assets.a", "Manifests/a.json"),
            MakeRef("assets.b", "Manifests/b.json"),
        });
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
        {"material.hero", 1001},
    });
    Resources::AssetRegistry registry;

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, DuplicatePackageAssetId);
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetRegistryBootstrapperTests, ExistingRegistryCollisionsFailWithoutMutation)
{
    const auto root = TempRoot("saga_runtime_registry_bootstrap_existing_collision");
    const auto packagePath = root / "package.json";
    WriteFile(root / "Manifests" / "assets.json",
              AssetManifestJson("texture.hero", "texture", "Cooked/a.ktx2"));
    WriteFile(root / "Manifests" / "Cooked" / "a.ktx2", "asset");
    const Packages::PackageManifest package = MakePackage(
        Packages::PackageKind::Client,
        {MakeRef("assets.main", "Manifests/assets.json")});
    StaticAssetIdResolver resolver = MakeResolver({
        {"texture.hero", 1001},
    });
    Resources::AssetRegistry registry;
    ASSERT_TRUE(registry.TryInsert(
        MakeRegistryEntry(1001, "texture.existing"))
            .Succeeded());
    ASSERT_TRUE(registry.TryInsert(
        MakeRegistryEntry(1002, "texture.hero"))
            .Succeeded());

    const auto result = RuntimeAssetRegistryBootstrapper::RegisterPackageAssets(
        registry, package, resolver, MakeOptions(packagePath));

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 2u);
    EXPECT_EQ(result.diagnostics[0].diagnosticId, RegistryAssetKeyCollision);
    EXPECT_EQ(result.diagnostics[1].diagnosticId, RegistryAssetIdCollision);
    EXPECT_EQ(registry.EntryCount(), 2u);
}
