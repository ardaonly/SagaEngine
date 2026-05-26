/// @file EditorDiagnosticsServiceTests.cpp
/// @brief Unit tests for SagaEditor diagnostics collection and subscriptions.

#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditor;

[[nodiscard]] EditorDiagnostic MakeDiagnostic(
    std::string source,
    EditorDiagnosticSeverity severity,
    std::string message)
{
    EditorDiagnostic diagnostic;
    diagnostic.source = std::move(source);
    diagnostic.severity = severity;
    diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] bool ContainsDiagnostic(
    const std::vector<EditorDiagnostic>& diagnostics,
    const std::string& source,
    const std::string& code,
    EditorDiagnosticSeverity severity,
    const std::string& message)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [&](const EditorDiagnostic& diagnostic)
        {
            return diagnostic.source == source &&
                   diagnostic.code == code &&
                   diagnostic.severity == severity &&
                   diagnostic.message == message;
        });
}

TEST(EditorDiagnosticsServiceTest, AddsDiagnosticsAndAssignsStableIds)
{
    EditorDiagnosticsService service;
    EditorDiagnostic diagnostic =
        MakeDiagnostic("asset-import",
                       EditorDiagnosticSeverity::Error,
                       "Texture import failed");
    diagnostic.code = "ASSET_IMPORT_FAILED";
    diagnostic.location.resource = "Textures/Hero.png";
    diagnostic.publishBlocking = true;

    const EditorDiagnosticId id = service.Add(diagnostic);

    ASSERT_NE(id, kInvalidEditorDiagnosticId);
    ASSERT_EQ(service.GetAll().size(), 1u);
    EXPECT_EQ(service.GetAll()[0].id, id);
    EXPECT_EQ(service.GetAll()[0].code, "ASSET_IMPORT_FAILED");
    EXPECT_TRUE(service.GetAll()[0].publishBlocking);
}

TEST(EditorDiagnosticsServiceTest, SubscribersReceiveInitialAndChangedSnapshots)
{
    EditorDiagnosticsService service;
    std::vector<std::size_t> snapshotSizes;

    const EditorDiagnosticsSubscription subscription = service.Subscribe(
        [&snapshotSizes](const std::vector<EditorDiagnostic>& diagnostics)
        {
            snapshotSizes.push_back(diagnostics.size());
        });

    ASSERT_NE(subscription, kInvalidEditorDiagnosticsSubscription);
    service.Add(MakeDiagnostic("sde",
                               EditorDiagnosticSeverity::Warning,
                               "Unused graph node"));
    service.Add(MakeDiagnostic("runtime-preview",
                               EditorDiagnosticSeverity::Info,
                               "Preview restarted"));

    ASSERT_EQ(snapshotSizes, (std::vector<std::size_t>{0, 1, 2}));

    EXPECT_TRUE(service.Unsubscribe(subscription));
    service.Clear();
    EXPECT_EQ(snapshotSizes, (std::vector<std::size_t>{0, 1, 2}));
}

TEST(EditorDiagnosticsServiceTest, ReplaceSourceRefreshesOneProducer)
{
    EditorDiagnosticsService service;
    service.Add(MakeDiagnostic("asset-import",
                               EditorDiagnosticSeverity::Error,
                               "Old asset error"));
    service.Add(MakeDiagnostic("sde",
                               EditorDiagnosticSeverity::Warning,
                               "Graph warning"));

    std::vector<EditorDiagnostic> refreshed;
    refreshed.push_back(MakeDiagnostic("",
                                       EditorDiagnosticSeverity::Error,
                                       "New asset error"));
    refreshed.push_back(MakeDiagnostic("asset-import",
                                       EditorDiagnosticSeverity::Warning,
                                       "Missing optional thumbnail"));

    EXPECT_EQ(service.ReplaceSource("asset-import", std::move(refreshed)), 2u);

    ASSERT_EQ(service.GetAll().size(), 3u);
    EXPECT_EQ(service.GetAll()[0].source, "sde");
    EXPECT_EQ(service.GetAll()[1].source, "asset-import");
    EXPECT_EQ(service.GetAll()[2].source, "asset-import");
    EXPECT_EQ(service.GetBySeverity(EditorDiagnosticSeverity::Error).size(), 1u);
    EXPECT_EQ(service.GetBySeverity(EditorDiagnosticSeverity::Warning).size(), 2u);
}

TEST(EditorDiagnosticsServiceTest, HostExposesSharedDiagnosticsService)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    IEditorDiagnosticsService& service = host.GetEditorDiagnosticsService();
    const EditorDiagnosticId id =
        service.Add(MakeDiagnostic("editor",
                                   EditorDiagnosticSeverity::Info,
                                   "Editor started"));

    EXPECT_NE(id, kInvalidEditorDiagnosticId);
    const std::vector<EditorDiagnostic>& diagnostics =
        host.GetEditorDiagnosticsService().GetAll();
    EXPECT_TRUE(ContainsDiagnostic(diagnostics,
                                   "editor",
                                   "",
                                   EditorDiagnosticSeverity::Info,
                                   "Editor started"));
    EXPECT_FALSE(ContainsDiagnostic(diagnostics,
                                    "editor.customization.workspace",
                                    "Customization.NoCompositionSnapshot",
                                    EditorDiagnosticSeverity::Error,
                                    "No usable resolved editor composition "
                                    "snapshot is available."));
    EXPECT_FALSE(ContainsDiagnostic(diagnostics,
                                    "editor.customization.shortcuts",
                                    "Customization.NoCompositionSnapshot",
                                    EditorDiagnosticSeverity::Error,
                                    "No usable resolved editor composition "
                                    "snapshot is available."));

    host.Shutdown();
}

} // namespace
