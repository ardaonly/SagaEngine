/// @file EditorProductionTests.cpp
/// @brief Production foundation tests for SagaEditor startup boundaries.

#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Customization/WorkspaceCustomizationOverlayPolicy.h"
#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace
{

using namespace SagaEditor;

namespace fs = std::filesystem;

[[nodiscard]] bool Contains(const std::vector<std::string>& values,
                            const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

void HashAppend(uint64_t& hash, const std::string& bytes)
{
    constexpr uint64_t kPrime = 1099511628211ull;
    for (unsigned char c : bytes)
    {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
}

[[nodiscard]] std::string StableHashBytes(const std::string& bytes)
{
    uint64_t hash = 14695981039346656037ull;
    HashAppend(hash, bytes);
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

[[nodiscard]] std::string StartupArtifactJson()
{
    return R"({
      "artifactKind": "saga.editor.composition",
      "artifactVersion": 1,
      "schemaPackage": "saga.editor",
      "schemaPackageVersion": 1,
      "compositionId": "saga.editor.default",
      "metadata": { "displayName": "Saga Default Editor" },
      "panels": [
        {
          "id": "saga.panel.hierarchy",
          "displayName": "Hierarchy",
          "kind": "builtin",
          "defaultVisible": true
        }
      ],
      "actions": [
        {
          "id": "saga.action.workspace.reset",
          "displayName": "Reset Workspace",
          "category": "Workspace",
          "safeInProduct": true
        }
      ],
      "menus": [],
      "toolbars": [
        {
          "id": "saga.toolbar.main",
          "displayName": "Main Toolbar",
          "actions": ["saga.action.workspace.reset"]
        }
      ],
      "shortcuts": [
        {
          "id": "saga.shortcut.workspace.reset",
          "actionId": "saga.action.workspace.reset",
          "chord": "Ctrl+Shift+R"
        }
      ],
      "workspaceLayouts": [
        {
          "id": "saga.layout.default",
          "displayName": "Default Layout"
        }
      ],
      "workspaces": [
        {
          "id": "saga.workspace.default",
          "displayName": "Default",
          "layoutId": "saga.layout.default",
          "visiblePanels": ["saga.panel.hierarchy"]
        }
      ],
      "editorModes": [
        {
          "id": "saga.mode.scene",
          "displayName": "Scene",
          "workspaceId": "saga.workspace.default",
          "requiredPanels": ["saga.panel.hierarchy"]
        }
      ]
    })";
}

[[nodiscard]] fs::path WriteStartupCompositionFixture(const fs::path& root)
{
    fs::remove_all(root);
    const std::string artifactText = StartupArtifactJson();
    WriteFile(root / "saga.editor.composition.json", artifactText);
    WriteFile(root / "saga.editor.composition.manifest.json",
              std::string{R"({
        "manifestKind": "saga.editor.composition.manifest",
        "manifestVersion": 1,
        "compositionId": "saga.editor.default",
        "artifactPath": "saga.editor.composition.json",
        "artifactHash": ")" + StableHashBytes(artifactText) + R"(",
        "schemaPackage": "saga.editor",
        "schemaPackageVersion": 1,
        "compilerVersion": "test"
      })"});
    return root / "saga.editor.composition.manifest.json";
}

[[nodiscard]] fs::path WriteStartupOverlayFixture(const fs::path& root)
{
    WriteFile(root / "user.workspace.overlay.json",
              R"({
        "schemaVersion": 1,
        "baseCompositionId": "saga.editor.default",
        "overlayId": "user.workspace.default",
        "layoutOverrides": [
          {
            "workspaceId": "saga.workspace.default",
            "panelId": "saga.panel.hierarchy",
            "placement": "left",
            "visible": false
          }
        ],
        "shortcutOverrides": [],
        "visibilityOverrides": [],
        "toolbarOverrides": []
      })");
    return root / "user.workspace.overlay.json";
}

[[nodiscard]] fs::path WriteDefaultWorkspaceOverlayFixture(
    const fs::path& workspaceRoot)
{
    const fs::path path =
        WorkspaceCustomizationOverlayPolicy().GetDefaultOverlayPath(
            workspaceRoot);
    WriteFile(path,
              R"({
        "schemaVersion": 1,
        "baseCompositionId": "saga.editor.default",
        "overlayId": "user.workspace.default",
        "layoutOverrides": [],
        "shortcutOverrides": [],
        "visibilityOverrides": [
          {
            "panelId": "saga.panel.hierarchy",
            "visible": false
          }
        ],
        "toolbarOverrides": []
      })");
    return path;
}

TEST(EditorProductionHostTest, EngineBridgeStartsReadyBehindEditorBoundary)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    const IEditorEngineBridge& bridge = host.GetEngineBridge();
    const EditorEngineBridgeSnapshot snapshot = bridge.Snapshot();

    EXPECT_EQ(bridge.StableIdentity(), "saga.editor.engine_bridge.local");
    EXPECT_EQ(snapshot.state, EditorEngineBridgeState::Ready);
    EXPECT_EQ(snapshot.displayName, "Local Engine Bridge");
    EXPECT_FALSE(snapshot.runtimeRole.empty());
    EXPECT_FALSE(snapshot.engineVersion.empty());

    host.Shutdown();
}

TEST(EditorProductionHostTest, StartsWithUnconfiguredCompositionSession)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    EXPECT_EQ(host.GetCompositionSession().Status(),
              EditorCompositionSessionStatus::Unconfigured);
    EXPECT_FALSE(host.GetCompositionSession().Snapshot().has_value());
    EXPECT_TRUE(host.GetCompositionSession().Diagnostics().empty());
    EXPECT_EQ(host.GetWorkspaceCustomizationSession().GetModel().status,
              WorkspaceCustomizationStatus::Unconfigured);

    host.Shutdown();
}

TEST(EditorProductionHostTest, LoadsCompiledCompositionManifestAtStartup)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_editor_host_composition_startup";
    EditorCompositionStartupConfig config;
    config.manifestPath = WriteStartupCompositionFixture(root);

    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                          fs::path{},
                          config));

    const EditorCompositionSession& session = host.GetCompositionSession();
    ASSERT_TRUE(session.IsUsable());
    ASSERT_TRUE(session.Snapshot().has_value());
    EXPECT_EQ(session.Snapshot()->compositionId, "saga.editor.default");
    ASSERT_EQ(session.Snapshot()->panels.size(), 1u);
    EXPECT_TRUE(session.Snapshot()->panels[0].visible);

    host.Shutdown();
}

TEST(EditorProductionHostTest, AppliesSafeCompositionOverlayAtStartup)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_editor_host_composition_overlay";
    EditorCompositionStartupConfig config;
    config.manifestPath = WriteStartupCompositionFixture(root);
    config.overlayPaths.push_back(WriteStartupOverlayFixture(root));

    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                          fs::path{},
                          config));

    const auto& snapshot = host.GetCompositionSession().Snapshot();
    ASSERT_TRUE(snapshot.has_value());
    ASSERT_EQ(snapshot->panels.size(), 1u);
    EXPECT_FALSE(snapshot->panels[0].visible);
    EXPECT_EQ(snapshot->panels[0].placement, "left");
    ASSERT_EQ(snapshot->appliedOverlayIds.size(), 1u);
    EXPECT_EQ(snapshot->appliedOverlayIds[0], "user.workspace.default");

    host.Shutdown();
}

TEST(EditorProductionHostTest, AutoDiscoversDefaultWorkspaceCustomizationOverlay)
{
    const fs::path compositionRoot =
        fs::temp_directory_path() / "saga_editor_host_composition_auto_overlay";
    const fs::path workspaceRoot =
        fs::temp_directory_path() / "saga_editor_host_workspace_auto_overlay";
    fs::remove_all(workspaceRoot);

    EditorCompositionStartupConfig config;
    config.manifestPath = WriteStartupCompositionFixture(compositionRoot);
    const fs::path overlayPath =
        WriteDefaultWorkspaceOverlayFixture(workspaceRoot);

    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                          workspaceRoot,
                          config));

    const auto& resolvedConfig = host.GetCompositionSession().Config();
    EXPECT_TRUE(std::find(resolvedConfig.overlayPaths.begin(),
                          resolvedConfig.overlayPaths.end(),
                          overlayPath) != resolvedConfig.overlayPaths.end());
    ASSERT_TRUE(host.GetCompositionSession().Snapshot().has_value());
    ASSERT_EQ(host.GetCompositionSession().Snapshot()->panels.size(), 1u);
    EXPECT_FALSE(host.GetCompositionSession().Snapshot()->panels[0].visible);
    EXPECT_EQ(host.GetWorkspaceCustomizationSession().OverlayPath(),
              overlayPath);
    EXPECT_TRUE(host.GetWorkspaceCustomizationSession().GetModel()
                    .restartRequired);

    host.Shutdown();
}

TEST(EditorProductionHostTest, RecordsCompositionStartupDiagnostics)
{
    EditorCompositionStartupConfig config;
    config.manifestPath =
        fs::temp_directory_path() / "missing_saga_editor_composition.manifest.json";

    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                          fs::path{},
                          config));

    EXPECT_EQ(host.GetCompositionSession().Status(),
              EditorCompositionSessionStatus::Failed);
    const std::vector<EditorDiagnostic>& diagnostics =
        host.GetEditorDiagnosticsService().GetAll();
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics[0].source, "editor.composition");
    EXPECT_EQ(diagnostics[0].code, "CompositionArtifact.FileOpenFailed");

    host.Shutdown();
}

TEST(EditorProductionHostTest, InvalidDefaultWorkspaceOverlayReportsCustomizationDiagnostics)
{
    const fs::path compositionRoot =
        fs::temp_directory_path() / "saga_editor_host_invalid_overlay_composition";
    const fs::path workspaceRoot =
        fs::temp_directory_path() / "saga_editor_host_invalid_overlay_workspace";
    fs::remove_all(workspaceRoot);

    EditorCompositionStartupConfig config;
    config.manifestPath = WriteStartupCompositionFixture(compositionRoot);
    WriteFile(WorkspaceCustomizationOverlayPolicy().GetDefaultOverlayPath(
                  workspaceRoot),
              "{ invalid json");

    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                          workspaceRoot,
                          config));

    const std::vector<EditorDiagnostic>& diagnostics =
        host.GetEditorDiagnosticsService().GetAll();
    const auto hasCustomizationParseDiagnostic =
        std::any_of(diagnostics.begin(),
                    diagnostics.end(),
                    [](const EditorDiagnostic& diagnostic)
                    {
                        return diagnostic.source ==
                                   "editor.customization.workspace" &&
                               diagnostic.code ==
                                   "CompositionOverlay.JsonParseFailed";
                    });
    EXPECT_TRUE(hasCustomizationParseDiagnostic);

    host.Shutdown();
}

TEST(EditorProductionHostTest, RequireCompositionFailsWithoutManifest)
{
    EditorCompositionStartupConfig config;
    config.requireValidComposition = true;

    EditorHost host;
    EXPECT_FALSE(host.Init(std::make_unique<MemoryEditorSettingsStore>(),
                           fs::path{},
                           config));

    host.Shutdown();
}

TEST(EditorProductionHostTest, ProfileSwitchDoesNotReplaceCoreOrEngineBridge)
{
    EditorHost host;
    ASSERT_TRUE(host.Init(std::make_unique<MemoryEditorSettingsStore>()));

    auto* selectionBefore = &host.GetSelectionManager();
    const std::string bridgeIdentityBefore =
        host.GetEngineBridge().StableIdentity();

    ASSERT_TRUE(host.GetEditorProfileRegistry().SetActive(
        "saga.profile.advanced_pipeline"));
    ASSERT_TRUE(host.GetEditorProfileRegistry().SetActive(
        "saga.profile.standard_pipeline"));

    EXPECT_EQ(selectionBefore, &host.GetSelectionManager());
    EXPECT_EQ(bridgeIdentityBefore, host.GetEngineBridge().StableIdentity());
    EXPECT_EQ(host.GetEngineBridge().Snapshot().state,
              EditorEngineBridgeState::Ready);

    host.Shutdown();
}

TEST(EditorProductionHostTest, BuiltinProfilesExposeProductionDashboard)
{
    const EditorProfile profiles[] = {
        MakeBasicProfile(),
        MakeNodeEditorProfile(),
        MakeStandardPipelineProfile(),
        MakeAdvancedPipelineProfile(),
        MakeCustomProfile(),
    };

    for (const EditorProfile& profile : profiles)
    {
        EXPECT_TRUE(Contains(profile.defaultPanels,
                             "saga.panel.production_dashboard"))
            << profile.id;
    }
}

TEST(EditorCustomizationCatalogTest, DefaultsToBuiltins)
{
    EditorCustomizationCatalog catalog;
    ASSERT_TRUE(catalog.Init({}));

    const EditorCustomizationStatus& status = catalog.Status();
    EXPECT_FALSE(status.loaded);
    EXPECT_NE(status.message.find("built-in"), std::string::npos);
    EXPECT_FALSE(status.sourceRoot.empty());
}

TEST(EditorCustomizationCatalogTest, LeavesProjectProfilesEmpty)
{
    EditorCustomizationCatalog catalog;
    const fs::path workspaceRoot =
        fs::path(SAGA_SOURCE_ROOT) / "Apps" / "Saga" /
        "Definitions" / "BasicWorkspace";

    ASSERT_TRUE(catalog.Init(workspaceRoot));

    const EditorCustomizationStatus& status = catalog.Status();
    EXPECT_EQ(status.sourceRoot.filename(), "EditorCustomization");
    EXPECT_FALSE(status.userConfigRoot.empty());
    EXPECT_TRUE(catalog.Snapshot().projectProfiles.empty());
}

} // namespace
