/// @file RuntimeStartupDiagnosticsTests.cpp
/// @brief Focused tests for Runtime startup diagnostic classification.

#include <SagaRuntime/RuntimeStartupDiagnostics.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

using SagaRuntime::RuntimeStartupDiagnosticCategory;
using SagaRuntime::RuntimeStartupDiagnostics;
using SagaRuntime::RuntimeStartupDiagnosticSeverity;
using SagaRuntime::RuntimeStartupPreflightDiagnostic;
using SagaRuntime::RuntimeStartupReport;
using SagaRuntime::RuntimeStartupReportState;

[[nodiscard]] RuntimeStartupPreflightDiagnostic MakeDiagnostic()
{
    RuntimeStartupPreflightDiagnostic diagnostic;
    diagnostic.diagnosticId = "Runtime.StartupPreflight.Test";
    diagnostic.message = "test startup diagnostic";
    diagnostic.manifestPath = "Package/package.json";
    diagnostic.fieldName = "assetManifests";
    diagnostic.referenceIndex = 2U;
    diagnostic.itemIndex = 3U;
    diagnostic.resourceId = "asset.main";
    diagnostic.resolvedPath = "Package/Manifests/assets.json";
    return diagnostic;
}

} // namespace

TEST(RuntimeStartupDiagnosticsTests, SkippedPreflightSummarizesAsAllowed)
{
    RuntimeStartupReport report;
    report.preflight.validationSkipped = true;

    const auto summary = RuntimeStartupDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeStartupReportState::PreflightSkipped);
    EXPECT_TRUE(summary.startupAllowed);
    EXPECT_TRUE(summary.preflightSkipped);
    EXPECT_EQ(summary.diagnosticCount, 0U);
    EXPECT_EQ(summary.blockingDiagnosticCount, 0U);
}

TEST(RuntimeStartupDiagnosticsTests, SuccessfulReportSummarizesAsReady)
{
    RuntimeStartupReport report;

    const auto summary = RuntimeStartupDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeStartupReportState::Ready);
    EXPECT_TRUE(summary.startupAllowed);
    EXPECT_FALSE(summary.preflightSkipped);
    EXPECT_EQ(summary.diagnosticCount, 0U);
    EXPECT_EQ(summary.blockingDiagnosticCount, 0U);
}

TEST(RuntimeStartupDiagnosticsTests, FailedReportSummarizesAsBlocked)
{
    RuntimeStartupReport report;
    report.preflight.diagnostics.push_back(MakeDiagnostic());

    const auto summary = RuntimeStartupDiagnostics::Summarize(report);

    EXPECT_EQ(summary.state, RuntimeStartupReportState::Blocked);
    EXPECT_FALSE(summary.startupAllowed);
    EXPECT_FALSE(summary.preflightSkipped);
    EXPECT_EQ(summary.diagnosticCount, 1U);
    EXPECT_EQ(summary.blockingDiagnosticCount, 1U);
}

TEST(RuntimeStartupDiagnosticsTests, DiagnosticViewPreservesPreflightMetadata)
{
    RuntimeStartupReport report;
    report.preflight.diagnostics.push_back(MakeDiagnostic());

    const auto views = RuntimeStartupDiagnostics::BuildDiagnosticViews(report);

    ASSERT_EQ(views.size(), 1U);
    const auto& view = views.front();
    EXPECT_EQ(view.severity, RuntimeStartupDiagnosticSeverity::Error);
    EXPECT_EQ(view.category, RuntimeStartupDiagnosticCategory::StartupPreflight);
    EXPECT_TRUE(view.blocking);
    EXPECT_EQ(view.diagnosticId, "Runtime.StartupPreflight.Test");
    EXPECT_EQ(view.message, "test startup diagnostic");
    EXPECT_EQ(view.manifestPath, std::filesystem::path("Package/package.json"));
    ASSERT_TRUE(view.fieldName.has_value());
    EXPECT_EQ(*view.fieldName, "assetManifests");
    ASSERT_TRUE(view.referenceIndex.has_value());
    EXPECT_EQ(*view.referenceIndex, 2U);
    ASSERT_TRUE(view.itemIndex.has_value());
    EXPECT_EQ(*view.itemIndex, 3U);
    ASSERT_TRUE(view.resourceId.has_value());
    EXPECT_EQ(*view.resourceId, "asset.main");
    ASSERT_TRUE(view.resolvedPath.has_value());
    EXPECT_EQ(*view.resolvedPath,
              std::filesystem::path("Package/Manifests/assets.json"));
}
