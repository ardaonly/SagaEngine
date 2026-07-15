/// @file RuntimeAssetStartupBootstrapTests.cpp
/// @brief Focused tests for the SagaRuntime asset bootstrap helper seam.

#include "RuntimeAssetStartupBootstrap.hpp"

#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace
{

namespace Resources = SagaEngine::Resources;

using SagaRuntime::RuntimeAssetBootstrapReportState;
using SagaRuntime::RuntimeSessionDescriptor;
using SagaRuntimeApp::RuntimeAssetStartupBootstrap;
using SagaRuntimeApp::RuntimeAssetStartupBootstrapResult;

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

[[nodiscard]] std::filesystem::path TempRoot(const char* name)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void CopyFixtureTo(const std::filesystem::path& destination)
{
    std::filesystem::copy(
        FixtureRoot(),
        destination,
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
}

[[nodiscard]] RuntimeSessionDescriptor DescriptorFor(
    const std::filesystem::path& packageManifestPath)
{
    RuntimeSessionDescriptor descriptor;
    descriptor.packageManifestPath = packageManifestPath;
    return descriptor;
}

} // namespace

TEST(RuntimeAssetStartupBootstrapTests, NoPackageManifestSkipsWithoutMutation)
{
    RuntimeSessionDescriptor descriptor;
    Resources::AssetRegistry registry;

    const RuntimeAssetStartupBootstrapResult result =
        RuntimeAssetStartupBootstrap::Bootstrap(descriptor, registry);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.bootstrapSkipped);
    EXPECT_EQ(result.summary.state, RuntimeAssetBootstrapReportState::Empty);
    EXPECT_EQ(result.bootstrap.registeredAssetCount, 0u);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetStartupBootstrapTests, RuntimeSmokePackageRegistersOneAsset)
{
    Resources::AssetRegistry registry;

    const RuntimeAssetStartupBootstrapResult result =
        RuntimeAssetStartupBootstrap::Bootstrap(
            DescriptorFor(FixturePackageManifestPath()),
            registry);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_FALSE(result.bootstrapSkipped);
    EXPECT_EQ(result.summary.state, RuntimeAssetBootstrapReportState::Ready);
    EXPECT_EQ(result.summary.registeredAssetCount, 1u);
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(registry.EntryCount(), 1u);

    const Resources::AssetRegistryEntry* byKey =
        registry.FindByKey(kRuntimeSmokeAssetKey);
    ASSERT_NE(byKey, nullptr);
    EXPECT_EQ(byKey->assetId, kRuntimeSmokeAssetId);

    const Resources::AssetRegistryEntry* byId =
        registry.Find(kRuntimeSmokeAssetId);
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->assetKey, kRuntimeSmokeAssetKey);
}

TEST(RuntimeAssetStartupBootstrapTests, MissingCookedAssetBlocksWithoutMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_app_asset_bootstrap_missing_cooked");
    CopyFixtureTo(root);
    std::filesystem::remove(
        root / "Manifests" / "Cooked" / "runtime_smoke_texture.ktx2");

    Resources::AssetRegistry registry;

    const RuntimeAssetStartupBootstrapResult result =
        RuntimeAssetStartupBootstrap::Bootstrap(
            DescriptorFor(root / "package_manifest.client.json"),
            registry);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.bootstrapSkipped);
    EXPECT_EQ(result.summary.state, RuntimeAssetBootstrapReportState::Blocked);
    EXPECT_EQ(result.summary.diagnosticCount, result.diagnostics.size());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(result.diagnostics.front().blocking);
    EXPECT_FALSE(result.diagnostics.front().diagnosticId.empty());
    EXPECT_TRUE(result.diagnostics.front().resolvedPath.has_value());
    EXPECT_EQ(registry.EntryCount(), 0u);
}

TEST(RuntimeAssetStartupBootstrapTests, BadPackageBaseDirectoryBlocksWithoutMutation)
{
    const std::filesystem::path root =
        TempRoot("saga_runtime_app_asset_bootstrap_bad_base");
    CopyFixtureTo(root);

    RuntimeSessionDescriptor descriptor =
        DescriptorFor(root / "package_manifest.client.json");
    descriptor.packageBaseDirectory = root / "WrongBase";

    Resources::AssetRegistry registry;

    const RuntimeAssetStartupBootstrapResult result =
        RuntimeAssetStartupBootstrap::Bootstrap(descriptor, registry);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.bootstrapSkipped);
    EXPECT_EQ(result.summary.state, RuntimeAssetBootstrapReportState::Blocked);
    EXPECT_EQ(result.summary.packageBaseDirectory, descriptor.packageBaseDirectory);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_FALSE(result.diagnostics.front().manifestPath.empty());
    EXPECT_EQ(registry.EntryCount(), 0u);
}
