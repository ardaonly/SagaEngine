/// @file RuntimeAssetBootstrapDiagnosticsTests.cpp
/// @brief Focused tests for Runtime asset bootstrap diagnostic classification.

#include <SagaRuntime/RuntimeAssetBootstrapDiagnostics.hpp>

#include <SagaEngine/Resources/AssetRegistry.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

namespace Resources = SagaEngine::Resources;

using SagaRuntime::RuntimeAssetBootstrapDiagnostic;
using SagaRuntime::RuntimeAssetBootstrapDiagnosticCategory;
using SagaRuntime::RuntimeAssetBootstrapDiagnostics;
using SagaRuntime::RuntimeAssetBootstrapDiagnosticSeverity;
using SagaRuntime::RuntimeAssetBootstrapOptions;
using SagaRuntime::RuntimeAssetBootstrapReportState;
using SagaRuntime::RuntimeAssetBootstrapResult;

[[nodiscard]] RuntimeAssetBootstrapDiagnostic MakeDiagnostic()
{
    RuntimeAssetBootstrapDiagnostic diagnostic;
    diagnostic.severity = RuntimeAssetBootstrapDiagnosticSeverity::Error;
    diagnostic.category =
        RuntimeAssetBootstrapDiagnosticCategory::AssetRegistryBootstrap;
    diagnostic.diagnosticId = "Runtime.AssetBootstrap.Test";
    diagnostic.message = "test asset bootstrap diagnostic";
    diagnostic.manifestPath = "Package/Manifests/assets.json";
    diagnostic.fieldName = "assets";
    diagnostic.referenceIndex = 2U;
    diagnostic.itemIndex = 3U;
    diagnostic.resourceId = "texture.runtime_smoke.albedo";
    diagnostic.assetId = 5001;
    diagnostic.resolvedPath =
        std::filesystem::path("Package/Manifests/Cooked/texture.ktx2");
    return diagnostic;
}

} // namespace

TEST(RuntimeAssetBootstrapDiagnosticsTests, SuccessfulBootstrapSummarizesAsReady)
{
    RuntimeAssetBootstrapResult result;
    result.registeredAssetCount = 1U;

    RuntimeAssetBootstrapOptions options;
    options.packageManifestPath = "Package/package_manifest.client.json";
    options.packageBaseDirectory = "Package";

    const auto summary =
        RuntimeAssetBootstrapDiagnostics::Summarize(result, options);

    EXPECT_EQ(summary.state, RuntimeAssetBootstrapReportState::Ready);
    EXPECT_TRUE(summary.succeeded);
    EXPECT_EQ(summary.registeredAssetCount, 1U);
    EXPECT_EQ(summary.diagnosticCount, 0U);
    EXPECT_EQ(summary.errorCount, 0U);
    EXPECT_EQ(summary.warningCount, 0U);
    EXPECT_EQ(
        summary.packageManifestPath,
        std::filesystem::path("Package/package_manifest.client.json"));
    EXPECT_EQ(summary.packageBaseDirectory, std::filesystem::path("Package"));
}

TEST(RuntimeAssetBootstrapDiagnosticsTests, EmptySuccessSummarizesAsEmpty)
{
    RuntimeAssetBootstrapResult result;

    const auto summary = RuntimeAssetBootstrapDiagnostics::Summarize(result);

    EXPECT_EQ(summary.state, RuntimeAssetBootstrapReportState::Empty);
    EXPECT_TRUE(summary.succeeded);
    EXPECT_EQ(summary.registeredAssetCount, 0U);
    EXPECT_EQ(summary.diagnosticCount, 0U);
    EXPECT_EQ(summary.errorCount, 0U);
    EXPECT_EQ(summary.warningCount, 0U);
}

TEST(RuntimeAssetBootstrapDiagnosticsTests, FailedBootstrapSummarizesAsBlocked)
{
    RuntimeAssetBootstrapResult result;
    result.diagnostics.push_back(MakeDiagnostic());

    const auto summary = RuntimeAssetBootstrapDiagnostics::Summarize(result);

    EXPECT_EQ(summary.state, RuntimeAssetBootstrapReportState::Blocked);
    EXPECT_FALSE(summary.succeeded);
    EXPECT_EQ(summary.registeredAssetCount, 0U);
    EXPECT_EQ(summary.diagnosticCount, 1U);
    EXPECT_EQ(summary.errorCount, 1U);
    EXPECT_EQ(summary.warningCount, 0U);
}

TEST(RuntimeAssetBootstrapDiagnosticsTests, DiagnosticViewsPreserveMetadata)
{
    RuntimeAssetBootstrapResult result;
    result.diagnostics.push_back(MakeDiagnostic());

    const auto views =
        RuntimeAssetBootstrapDiagnostics::BuildDiagnosticViews(result);

    ASSERT_EQ(views.size(), 1U);
    const auto& view = views.front();
    EXPECT_EQ(view.severity, RuntimeAssetBootstrapDiagnosticSeverity::Error);
    EXPECT_EQ(
        view.category,
        RuntimeAssetBootstrapDiagnosticCategory::AssetRegistryBootstrap);
    EXPECT_TRUE(view.blocking);
    EXPECT_EQ(view.diagnosticId, "Runtime.AssetBootstrap.Test");
    EXPECT_EQ(view.message, "test asset bootstrap diagnostic");
    EXPECT_EQ(
        view.manifestPath,
        std::filesystem::path("Package/Manifests/assets.json"));
    ASSERT_TRUE(view.fieldName.has_value());
    EXPECT_EQ(*view.fieldName, "assets");
    ASSERT_TRUE(view.referenceIndex.has_value());
    EXPECT_EQ(*view.referenceIndex, 2U);
    ASSERT_TRUE(view.itemIndex.has_value());
    EXPECT_EQ(*view.itemIndex, 3U);
    ASSERT_TRUE(view.resourceId.has_value());
    EXPECT_EQ(*view.resourceId, "texture.runtime_smoke.albedo");
    ASSERT_TRUE(view.assetId.has_value());
    EXPECT_EQ(*view.assetId, 5001U);
    ASSERT_TRUE(view.resolvedPath.has_value());
    EXPECT_EQ(
        *view.resolvedPath,
        std::filesystem::path("Package/Manifests/Cooked/texture.ktx2"));
}

TEST(RuntimeAssetBootstrapDiagnosticsTests, DiagnosticsFacadeDoesNotMutateRegistry)
{
    Resources::AssetRegistry registry;
    RuntimeAssetBootstrapResult result;
    result.diagnostics.push_back(MakeDiagnostic());

    const auto summary = RuntimeAssetBootstrapDiagnostics::Summarize(result);
    const auto views =
        RuntimeAssetBootstrapDiagnostics::BuildDiagnosticViews(result);

    EXPECT_EQ(summary.state, RuntimeAssetBootstrapReportState::Blocked);
    EXPECT_EQ(views.size(), 1U);
    EXPECT_EQ(registry.EntryCount(), 0U);
}
