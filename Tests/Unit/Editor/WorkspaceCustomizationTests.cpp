/// @file WorkspaceCustomizationTests.cpp
/// @brief Unit tests for safe workspace customization infrastructure.

#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"
#include "SagaEditor/Composition/EditorCompositionDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationCapability.h"
#include "SagaEditor/Customization/EditorCustomizationOverlayLoader.h"
#include "SagaEditor/Customization/ShortcutCustomizationController.h"
#include "SagaEditor/Customization/ShortcutCustomizationFeedback.h"
#include "SagaEditor/Customization/ShortcutCustomizationPanelViewModel.h"
#include "SagaEditor/Customization/ShortcutCustomizationSession.h"
#include "SagaEditor/Customization/WorkspaceCustomizationController.h"
#include "SagaEditor/Customization/WorkspaceCustomizationFeedback.h"
#include "SagaEditor/Customization/WorkspaceCustomizationPanelViewModel.h"
#include "SagaEditor/Customization/WorkspaceCustomizationOverlayPolicy.h"
#include "SagaEditor/Customization/WorkspaceCustomizationOverlayStore.h"
#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"
#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

using namespace SagaEditor;
namespace fs = std::filesystem;

[[nodiscard]] ResolvedEditorCompositionSnapshot MakeSnapshot()
{
    ResolvedEditorCompositionSnapshot snapshot;
    snapshot.compositionId = "saga.editor.default";
    snapshot.artifactHash = "artifact-hash";
    snapshot.isUsable = true;
    snapshot.panels.push_back({ { "saga.panel.hierarchy",
                                  "Hierarchy",
                                  "builtin",
                                  true,
                                  false,
                                  {} },
                                true,
                                "left" });
    snapshot.panels.push_back({ { "saga.panel.console",
                                  "Console",
                                  "builtin",
                                  false,
                                  false,
                                  {} },
                                false,
                                "bottom" });
    snapshot.panels.push_back({ { "saga.panel.internal.serviceGraph",
                                  "Service Graph",
                                  "builtin",
                                  false,
                                  true,
                                  {} },
                                false,
                                "right" });
    snapshot.panels.push_back({ { "saga.panel.missing",
                                  "Missing Implementation",
                                  "builtin",
                                  true,
                                  false,
                                  {} },
                                true,
                                "right" });
    snapshot.actions.push_back({ "saga.action.workspace.reset",
                                 "Reset Workspace",
                                 "Workspace",
                                 true,
                                 false });
    snapshot.actions.push_back({ "saga.command.file.save",
                                 "Save Scene",
                                 "File",
                                 true,
                                 false });
    snapshot.actions.push_back({ "saga.command.edit.undo",
                                 "Undo",
                                 "Edit",
                                 true,
                                 false });
    snapshot.actions.push_back({ "saga.action.internal.mutate",
                                 "Mutate Service Graph",
                                 "Internal",
                                 false,
                                 true });
    snapshot.shortcuts.push_back({ "saga.shortcut.file.save",
                                   "saga.command.file.save",
                                   "Ctrl+S" });
    snapshot.shortcuts.push_back({ "saga.shortcut.edit.undo",
                                   "saga.command.edit.undo",
                                   "Ctrl+Z" });
    snapshot.workspaces.push_back({ "saga.workspace.default",
                                    "Default",
                                    "saga.layout.default",
                                    { "saga.panel.hierarchy" } });
    return snapshot;
}

[[nodiscard]] EditorCustomizationAvailability MakeAvailability()
{
    EditorCustomizationAvailability availability;
    availability.registeredPanelIds = {
        "saga.panel.hierarchy",
        "saga.panel.console",
        "saga.panel.internal.serviceGraph",
    };
    availability.availableActionIds = {
        "saga.action.workspace.reset",
        "saga.command.file.save",
        "saga.command.edit.undo",
    };
    availability.shortcutAssignableActionIds = {
        "saga.action.workspace.reset",
        "saga.command.file.save",
        "saga.command.edit.undo",
    };
    availability.activeWorkspaceId = "saga.workspace.default";
    return availability;
}

[[nodiscard]] const WorkspaceCustomizationPanelState* FindPanel(
    const WorkspaceCustomizationModel& model,
    const std::string& id)
{
    for (const WorkspaceCustomizationPanelState& panel : model.panels)
    {
        if (panel.id == id)
        {
            return &panel;
        }
    }
    return nullptr;
}

[[nodiscard]] bool HasCode(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics,
    const std::string& code)
{
    for (const EditorCustomizationDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.code == code)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const WorkspaceCustomizationPanelRow* FindViewRow(
    const WorkspaceCustomizationPanelViewState& state,
    const std::string& id)
{
    for (const WorkspaceCustomizationPanelRow& row : state.panels)
    {
        if (row.id == id)
        {
            return &row;
        }
    }
    return nullptr;
}

[[nodiscard]] const ShortcutCustomizationActionState* FindShortcutAction(
    const ShortcutCustomizationModel& model,
    const std::string& id)
{
    for (const ShortcutCustomizationActionState& action : model.actions)
    {
        if (action.actionId == id)
        {
            return &action;
        }
    }
    return nullptr;
}

[[nodiscard]] const ShortcutCustomizationActionRow* FindShortcutViewRow(
    const ShortcutCustomizationPanelViewState& state,
    const std::string& id)
{
    for (const ShortcutCustomizationActionRow& row : state.actions)
    {
        if (row.actionId == id)
        {
            return &row;
        }
    }
    return nullptr;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

TEST(WorkspaceCustomizationCapabilityTests, NoSnapshotProducesUnconfiguredState)
{
    WorkspaceCustomizationControllerConfig config;
    config.availability = MakeAvailability();

    WorkspaceCustomizationController controller(config);

    EXPECT_EQ(controller.GetModel().status,
              WorkspaceCustomizationStatus::Unconfigured);
    EXPECT_TRUE(controller.GetModel().panels.empty());
    EXPECT_TRUE(HasCode(controller.GetModel().diagnostics,
                        "Customization.NoCompositionSnapshot"));
}

TEST(WorkspaceCustomizationCapabilityTests, SnapshotPanelsExposeSafeCapabilities)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    EditorCustomizationCapabilityModel capabilities =
        BuildEditorCustomizationCapabilities(&snapshot, MakeAvailability());

    ASSERT_TRUE(capabilities.hasUsableSnapshot);
    ASSERT_EQ(capabilities.panels.size(), 4u);
    EXPECT_EQ(capabilities.panels[0].id, "saga.panel.hierarchy");
    EXPECT_EQ(capabilities.panels[0].displayName, "Hierarchy");
    EXPECT_TRUE(capabilities.panels[0].defaultVisible);
    EXPECT_TRUE(capabilities.panels[0].currentVisible);
    EXPECT_TRUE(capabilities.panels[0].editable);
    EXPECT_FALSE(capabilities.panels[2].editable);
    EXPECT_EQ(capabilities.panels[2].lockedReason,
              EditorCustomizationLockReason::InternalOnly);
    EXPECT_FALSE(capabilities.panels[3].editable);
    EXPECT_EQ(capabilities.panels[3].lockedReason,
              EditorCustomizationLockReason::UnavailableImplementation);
    ASSERT_EQ(capabilities.actions.size(), 4u);
    EXPECT_TRUE(capabilities.actions[0].shortcutAssignable);
    EXPECT_TRUE(capabilities.actions[1].shortcutAssignable);
    EXPECT_TRUE(capabilities.actions[2].shortcutAssignable);
    EXPECT_FALSE(capabilities.actions[3].productSafe);
    ASSERT_EQ(capabilities.workspaces.size(), 1u);
    EXPECT_TRUE(capabilities.workspaces[0].active);
}

TEST(WorkspaceCustomizationControllerTests, ToggleSafePanelCreatesRestartRequiredDelta)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    WorkspaceCustomizationController controller(config);

    ASSERT_TRUE(controller.TogglePanelVisibility("saga.panel.hierarchy",
                                                 false));

    const WorkspaceCustomizationModel& model = controller.GetModel();
    EXPECT_TRUE(model.dirty);
    EXPECT_TRUE(model.restartRequired);
    EXPECT_FALSE(model.liveApplyAvailable);
    const WorkspaceCustomizationPanelState* panel =
        FindPanel(model, "saga.panel.hierarchy");
    ASSERT_NE(panel, nullptr);
    ASSERT_TRUE(panel->pendingVisible.has_value());
    EXPECT_FALSE(*panel->pendingVisible);

    EditorCustomizationOverlay overlay = controller.BuildOverlayDelta();
    ASSERT_EQ(overlay.visibilityOverrides.size(), 1u);
    EXPECT_EQ(overlay.visibilityOverrides[0].panelId,
              "saga.panel.hierarchy");
    EXPECT_FALSE(overlay.visibilityOverrides[0].visible);
    EXPECT_EQ(overlay.baseCompositionId, "saga.editor.default");
    EXPECT_EQ(overlay.baseArtifactHash, "artifact-hash");
}

TEST(WorkspaceCustomizationControllerTests, InitialOverlayUpdatesPendingVisibleState)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    overlay.baseCompositionId = "saga.editor.default";
    overlay.overlayId = "user.workspace.default";
    overlay.layoutOverrides.push_back({ "saga.workspace.default",
                                        "saga.panel.hierarchy",
                                        "left",
                                        false });

    WorkspaceCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    config.initialOverlay = overlay;
    WorkspaceCustomizationController controller(config);

    const WorkspaceCustomizationPanelState* panel =
        FindPanel(controller.GetModel(), "saga.panel.hierarchy");
    ASSERT_NE(panel, nullptr);
    ASSERT_TRUE(panel->pendingVisible.has_value());
    EXPECT_FALSE(*panel->pendingVisible);
    EXPECT_TRUE(controller.GetModel().dirty);
    EXPECT_TRUE(controller.GetModel().restartRequired);
}

TEST(WorkspaceCustomizationControllerTests, ToggleSameValueTwiceIsDeterministic)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    WorkspaceCustomizationController controller(config);

    EXPECT_TRUE(controller.TogglePanelVisibility("saga.panel.hierarchy",
                                                false));
    EXPECT_TRUE(controller.TogglePanelVisibility("saga.panel.hierarchy",
                                                false));

    EditorCustomizationOverlay overlay = controller.BuildOverlayDelta();
    ASSERT_EQ(overlay.visibilityOverrides.size(), 1u);
    EXPECT_FALSE(overlay.visibilityOverrides[0].visible);
}

TEST(WorkspaceCustomizationControllerTests, LockedPanelOperationsFail)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    WorkspaceCustomizationController controller(config);

    EXPECT_FALSE(controller.TogglePanelVisibility(
        "saga.panel.internal.serviceGraph",
        true));
    EXPECT_FALSE(controller.TogglePanelVisibility("saga.panel.missing",
                                                 false));
    EXPECT_FALSE(controller.TogglePanelVisibility("saga.panel.unknown",
                                                 false));

    EXPECT_TRUE(HasCode(controller.GetDiagnostics(),
                        "Customization.InternalOnlyPanel"));
    EXPECT_TRUE(HasCode(controller.GetDiagnostics(),
                        "Customization.UnavailablePanelImplementation"));
    EXPECT_TRUE(HasCode(controller.GetDiagnostics(),
                        "Customization.UnknownPanel"));
    EXPECT_TRUE(controller.BuildOverlayDelta().visibilityOverrides.empty());
}

TEST(WorkspaceCustomizationControllerTests, ResetPanelAndWorkspaceClearOverrides)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    WorkspaceCustomizationController controller(config);

    ASSERT_TRUE(controller.TogglePanelVisibility("saga.panel.hierarchy",
                                                 false));
    ASSERT_TRUE(controller.TogglePanelVisibility("saga.panel.console",
                                                 true));
    EXPECT_TRUE(controller.ResetPanel("saga.panel.hierarchy"));
    EXPECT_EQ(controller.BuildOverlayDelta().visibilityOverrides.size(), 1u);

    controller.ResetWorkspace();
    EXPECT_TRUE(controller.BuildOverlayDelta().visibilityOverrides.empty());
    EXPECT_FALSE(controller.GetModel().dirty);
    EXPECT_FALSE(controller.GetModel().restartRequired);
}

TEST(WorkspaceCustomizationOverlayStoreTests, PolicyUsesUserCustomizationPath)
{
    WorkspaceCustomizationOverlayPolicy policy;
    EXPECT_EQ(policy.GetDefaultOverlayPath("/workspace").generic_string(),
              "/workspace/.saga/editor/customization/workspace.overlay.json");
}

TEST(WorkspaceCustomizationOverlayStoreTests, SavedOverlayLoadsThroughExistingLoader)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_store";
    fs::remove_all(root);
    const fs::path path =
        WorkspaceCustomizationOverlayPolicy().GetDefaultOverlayPath(root);

    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    overlay.baseCompositionId = "saga.editor.default";
    overlay.baseArtifactHash = "artifact-hash";
    overlay.overlayId = "user.workspace.default";
    overlay.visibilityOverrides.push_back({ "saga.panel.hierarchy", false });

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayStoreResult saveResult =
        store.Save(path, overlay);
    ASSERT_TRUE(saveResult.succeeded);

    EditorCustomizationOverlayLoader loader;
    EditorCustomizationOverlayLoadResult loadResult =
        loader.LoadFile(path);
    ASSERT_TRUE(loadResult.overlay.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(loadResult.diagnostics));
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_FALSE(loadResult.overlay->visibilityOverrides[0].visible);

    WorkspaceCustomizationOverlayLoadResult storeLoad = store.Load(path);
    ASSERT_TRUE(storeLoad.overlay.has_value());
    EXPECT_FALSE(storeLoad.fileMissing);
    EXPECT_TRUE(storeLoad.diagnostics.empty());
}

TEST(WorkspaceCustomizationOverlayStoreTests, MissingInvalidAndResetBehavior)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_store_reset";
    fs::remove_all(root);
    const fs::path path =
        WorkspaceCustomizationOverlayPolicy().GetDefaultOverlayPath(root);

    WorkspaceCustomizationOverlayStore store;
    WorkspaceCustomizationOverlayLoadResult missing = store.Load(path);
    EXPECT_TRUE(missing.fileMissing);
    EXPECT_FALSE(missing.overlay.has_value());

    WriteFile(path, "{ invalid json");
    WorkspaceCustomizationOverlayLoadResult invalid = store.Load(path);
    EXPECT_FALSE(invalid.overlay.has_value());
    EXPECT_TRUE(HasErrorCustomizationDiagnostic(invalid.diagnostics));

    WorkspaceCustomizationOverlayStoreResult reset = store.Reset(path);
    EXPECT_TRUE(reset.succeeded);
    EXPECT_FALSE(fs::exists(path));
}

TEST(WorkspaceCustomizationSessionTests, SaveAndResetOperateOnSafeOverlayStore)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_session";
    fs::remove_all(root);
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    EditorDiagnosticsService diagnostics;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    config.availability.registeredPanelIds.push_back("saga.panel.missing");
    config.diagnosticsService = &diagnostics;

    WorkspaceCustomizationSession session;
    ASSERT_TRUE(session.Init(std::move(config)));
    ASSERT_NE(session.GetController(), nullptr);
    ASSERT_TRUE(session.GetController()->TogglePanelVisibility(
        "saga.panel.hierarchy",
        false));

    WorkspaceCustomizationSessionResult saveResult = session.SaveOverlay();
    ASSERT_TRUE(saveResult.succeeded);
    EXPECT_TRUE(fs::exists(session.OverlayPath()));

    EditorCustomizationOverlayLoader loader;
    EditorCustomizationOverlayLoadResult loadResult =
        loader.LoadFile(session.OverlayPath());
    ASSERT_TRUE(loadResult.overlay.has_value());
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_EQ(loadResult.overlay->visibilityOverrides[0].panelId,
              "saga.panel.hierarchy");

    WorkspaceCustomizationSessionResult resetResult = session.ResetOverlay();
    EXPECT_TRUE(resetResult.succeeded);
    EXPECT_FALSE(fs::exists(session.OverlayPath()));
    EXPECT_FALSE(session.GetModel().dirty);
    EXPECT_TRUE(diagnostics.GetAll().empty());
}

TEST(WorkspaceCustomizationSessionTests, ExportWritesLoadableOverlay)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_export";
    fs::remove_all(root);
    const fs::path exportPath = root / "exported.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));
    ASSERT_NE(session.GetController(), nullptr);
    ASSERT_TRUE(session.GetController()->TogglePanelVisibility(
        "saga.panel.hierarchy",
        false));

    WorkspaceCustomizationSessionResult exportResult =
        session.ExportOverlay(exportPath);
    ASSERT_TRUE(exportResult.succeeded);

    EditorCustomizationOverlayLoader loader;
    EditorCustomizationOverlayLoadResult loadResult =
        loader.LoadFile(exportPath);
    ASSERT_TRUE(loadResult.overlay.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(loadResult.diagnostics));
    EXPECT_EQ(loadResult.overlay->baseCompositionId, "saga.editor.default");
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_EQ(loadResult.overlay->visibilityOverrides[0].panelId,
              "saga.panel.hierarchy");
    EXPECT_FALSE(loadResult.overlay->visibilityOverrides[0].visible);
}

TEST(WorkspaceCustomizationSessionTests, ImportValidOverlayReplacesDefaultOverlay)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_import";
    fs::remove_all(root);
    const fs::path importPath = root / "incoming.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    EditorCustomizationOverlay incoming;
    incoming.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    incoming.baseCompositionId = "saga.editor.default";
    incoming.baseArtifactHash = "artifact-hash";
    incoming.overlayId = "imported.workspace";
    incoming.visibilityOverrides.push_back({ "saga.panel.console", true });

    WorkspaceCustomizationOverlayStore store;
    ASSERT_TRUE(store.Save(importPath, incoming).succeeded);

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationSessionResult importResult =
        session.ImportOverlay(importPath);
    ASSERT_TRUE(importResult.succeeded);
    EXPECT_TRUE(fs::exists(session.OverlayPath()));

    const WorkspaceCustomizationPanelState* console =
        FindPanel(session.GetModel(), "saga.panel.console");
    ASSERT_NE(console, nullptr);
    ASSERT_TRUE(console->pendingVisible.has_value());
    EXPECT_TRUE(*console->pendingVisible);
    EXPECT_TRUE(session.GetModel().restartRequired);

    EditorCustomizationOverlayLoadResult loadResult =
        EditorCustomizationOverlayLoader().LoadFile(session.OverlayPath());
    ASSERT_TRUE(loadResult.overlay.has_value());
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_EQ(loadResult.overlay->visibilityOverrides[0].panelId,
              "saga.panel.console");
}

TEST(WorkspaceCustomizationSessionTests, ImportInvalidJsonPreservesCurrentOverlay)
{
    const fs::path root =
        fs::temp_directory_path() /
        "saga_workspace_customization_import_invalid";
    fs::remove_all(root);
    const fs::path importPath = root / "invalid.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));
    ASSERT_NE(session.GetController(), nullptr);
    ASSERT_TRUE(session.GetController()->TogglePanelVisibility(
        "saga.panel.hierarchy",
        false));
    ASSERT_TRUE(session.SaveOverlay().succeeded);

    WriteFile(importPath, "{ invalid json");
    WorkspaceCustomizationSessionResult importResult =
        session.ImportOverlay(importPath);
    EXPECT_FALSE(importResult.succeeded);
    EXPECT_TRUE(HasErrorCustomizationDiagnostic(importResult.diagnostics));

    EditorCustomizationOverlayLoadResult loadResult =
        EditorCustomizationOverlayLoader().LoadFile(session.OverlayPath());
    ASSERT_TRUE(loadResult.overlay.has_value());
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_EQ(loadResult.overlay->visibilityOverrides[0].panelId,
              "saga.panel.hierarchy");
}

TEST(WorkspaceCustomizationSessionTests, ImportUnsafeOverlayIsRejected)
{
    const fs::path root =
        fs::temp_directory_path() /
        "saga_workspace_customization_import_unsafe";
    fs::remove_all(root);
    const fs::path importPath = root / "unsafe.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    EditorCustomizationOverlay incoming;
    incoming.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    incoming.baseCompositionId = "saga.editor.default";
    incoming.baseArtifactHash = "artifact-hash";
    incoming.overlayId = "imported.workspace";
    incoming.visibilityOverrides.push_back(
        { "saga.panel.internal.serviceGraph", true });

    WorkspaceCustomizationOverlayStore store;
    ASSERT_TRUE(store.Save(importPath, incoming).succeeded);

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationSessionResult importResult =
        session.ImportOverlay(importPath);
    EXPECT_FALSE(importResult.succeeded);
    EXPECT_TRUE(HasCode(importResult.diagnostics,
                        "Customization.InternalOnlyPanel"));
    EXPECT_FALSE(fs::exists(session.OverlayPath()));
}

TEST(WorkspaceCustomizationSessionTests, ImportRejectsNonWorkspaceOverrides)
{
    const fs::path root =
        fs::temp_directory_path() /
        "saga_workspace_customization_import_shortcuts";
    fs::remove_all(root);
    const fs::path importPath = root / "shortcut.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    EditorCustomizationOverlay incoming;
    incoming.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    incoming.baseCompositionId = "saga.editor.default";
    incoming.baseArtifactHash = "artifact-hash";
    incoming.overlayId = "imported.workspace";
    incoming.shortcutOverrides.push_back(
        { "saga.action.workspace.reset", "Ctrl+R" });

    WorkspaceCustomizationOverlayStore store;
    ASSERT_TRUE(store.Save(importPath, incoming).succeeded);

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationSessionResult importResult =
        session.ImportOverlay(importPath);
    EXPECT_FALSE(importResult.succeeded);
    EXPECT_TRUE(HasCode(importResult.diagnostics,
                        "Customization.InvalidOverlay"));
    EXPECT_FALSE(fs::exists(session.OverlayPath()));
}

TEST(WorkspaceCustomizationSessionTests, InvalidOverlayPublishesDiagnostics)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_session_invalid";
    fs::remove_all(root);
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    const fs::path overlayPath =
        WorkspaceCustomizationOverlayPolicy().GetDefaultOverlayPath(root);
    WriteFile(overlayPath, "{ invalid json");

    EditorDiagnosticsService diagnostics;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    config.diagnosticsService = &diagnostics;

    WorkspaceCustomizationSession session;
    ASSERT_TRUE(session.Init(std::move(config)));

    const auto hasParseDiagnostic =
        std::any_of(diagnostics.GetAll().begin(),
                    diagnostics.GetAll().end(),
                    [](const EditorDiagnostic& diagnostic)
                    {
                        return diagnostic.source ==
                                   "editor.customization.workspace" &&
                               diagnostic.code ==
                                   "CompositionOverlay.JsonParseFailed";
                    });
    EXPECT_TRUE(hasParseDiagnostic);
}

TEST(WorkspaceCustomizationPanelViewModelTests, UnconfiguredSessionProducesEmptyState)
{
    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    const WorkspaceCustomizationPanelViewState state = viewModel.GetState();

    EXPECT_EQ(state.status, WorkspaceCustomizationStatus::Unconfigured);
    EXPECT_EQ(state.statusText, "Unconfigured");
    EXPECT_TRUE(state.panels.empty());
    EXPECT_FALSE(state.canSave);
    EXPECT_FALSE(state.restartRequired);
}

TEST(WorkspaceCustomizationPanelViewModelTests, ReadySessionListsRowsAndLocks)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    const WorkspaceCustomizationPanelViewState state = viewModel.GetState();

    EXPECT_EQ(state.status, WorkspaceCustomizationStatus::Ready);
    ASSERT_EQ(state.panels.size(), 4u);

    const WorkspaceCustomizationPanelRow* hierarchy =
        FindViewRow(state, "saga.panel.hierarchy");
    ASSERT_NE(hierarchy, nullptr);
    EXPECT_TRUE(hierarchy->editable);
    EXPECT_TRUE(hierarchy->visible);
    EXPECT_TRUE(hierarchy->defaultVisible);

    const WorkspaceCustomizationPanelRow* internal =
        FindViewRow(state, "saga.panel.internal.serviceGraph");
    ASSERT_NE(internal, nullptr);
    EXPECT_FALSE(internal->editable);
    EXPECT_EQ(internal->lockedReason, "Internal only");

    const WorkspaceCustomizationPanelRow* missing =
        FindViewRow(state, "saga.panel.missing");
    ASSERT_NE(missing, nullptr);
    EXPECT_FALSE(missing->editable);
    EXPECT_EQ(missing->lockedReason, "Implementation unavailable");
}

TEST(WorkspaceCustomizationPanelViewModelTests, ToggleSafePanelUpdatesPendingState)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    ASSERT_TRUE(viewModel.TogglePanel("saga.panel.hierarchy", false));

    const WorkspaceCustomizationPanelViewState state = viewModel.GetState();
    const WorkspaceCustomizationPanelRow* hierarchy =
        FindViewRow(state, "saga.panel.hierarchy");
    ASSERT_NE(hierarchy, nullptr);
    EXPECT_FALSE(hierarchy->visible);
    EXPECT_TRUE(hierarchy->pending);
    EXPECT_TRUE(state.dirty);
    EXPECT_TRUE(state.restartRequired);
}

TEST(WorkspaceCustomizationPanelViewModelTests, ToggleLockedPanelReportsDiagnostic)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    EXPECT_FALSE(viewModel.TogglePanel("saga.panel.internal.serviceGraph",
                                       true));

    const WorkspaceCustomizationPanelViewState state = viewModel.GetState();
    EXPECT_EQ(state.status, WorkspaceCustomizationStatus::Invalid);
    EXPECT_FALSE(state.dirty);
    EXPECT_FALSE(state.diagnostics.empty());
}

TEST(WorkspaceCustomizationPanelViewModelTests, SaveAndResetUseSessionStore)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_workspace_customization_view_model";
    fs::remove_all(root);
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    config.availability.registeredPanelIds.push_back("saga.panel.missing");
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    ASSERT_TRUE(viewModel.TogglePanel("saga.panel.hierarchy", false));
    WorkspaceCustomizationSessionResult saveResult = viewModel.Save();
    ASSERT_TRUE(saveResult.succeeded);
    EXPECT_TRUE(fs::exists(session.OverlayPath()));
    EXPECT_TRUE(viewModel.GetState().restartRequired);

    WorkspaceCustomizationSessionResult resetResult =
        viewModel.ResetWorkspace();
    ASSERT_TRUE(resetResult.succeeded);
    EXPECT_FALSE(fs::exists(session.OverlayPath()));
    EXPECT_FALSE(viewModel.GetState().dirty);
}

TEST(WorkspaceCustomizationPanelViewModelTests, ImportAndExportUseSessionStore)
{
    const fs::path root =
        fs::temp_directory_path() /
        "saga_workspace_customization_view_model_import_export";
    fs::remove_all(root);
    const fs::path exportPath = root / "export.overlay.json";
    const fs::path importPath = root / "import.overlay.json";
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    WorkspaceCustomizationSession session;
    WorkspaceCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    WorkspaceCustomizationPanelViewModel viewModel(session);
    ASSERT_TRUE(viewModel.TogglePanel("saga.panel.hierarchy", false));
    WorkspaceCustomizationSessionResult exportResult =
        viewModel.ExportOverlay(exportPath);
    ASSERT_TRUE(exportResult.succeeded);
    ASSERT_TRUE(fs::exists(exportPath));

    EditorCustomizationOverlay incoming;
    incoming.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    incoming.baseCompositionId = "saga.editor.default";
    incoming.baseArtifactHash = "artifact-hash";
    incoming.overlayId = "imported.workspace";
    incoming.visibilityOverrides.push_back({ "saga.panel.console", true });
    ASSERT_TRUE(WorkspaceCustomizationOverlayStore()
                    .Save(importPath, incoming)
                    .succeeded);

    WorkspaceCustomizationSessionResult importResult =
        viewModel.ImportOverlay(importPath);
    ASSERT_TRUE(importResult.succeeded);

    const WorkspaceCustomizationPanelViewState state = viewModel.GetState();
    const WorkspaceCustomizationPanelRow* console =
        FindViewRow(state, "saga.panel.console");
    ASSERT_NE(console, nullptr);
    EXPECT_TRUE(console->visible);
    EXPECT_TRUE(console->pending);
    EXPECT_TRUE(state.restartRequired);
}

TEST(WorkspaceCustomizationFeedbackTests, SuccessMessageIncludesRestartPolicy)
{
    WorkspaceCustomizationSessionResult result;
    result.succeeded = true;

    const EditorNotification notification =
        MakeWorkspaceCustomizationNotification(
            WorkspaceCustomizationFeedbackOperation::Save,
            result,
            true);

    EXPECT_EQ(notification.source, kWorkspaceCustomizationNotificationSource);
    EXPECT_EQ(notification.severity, EditorNotificationSeverity::Success);
    EXPECT_EQ(notification.message,
              "Workspace customization saved. Restart editor to apply workspace changes.");
    EXPECT_TRUE(notification.detail.empty());
}

TEST(WorkspaceCustomizationFeedbackTests, ExportSuccessDoesNotForceRestartText)
{
    WorkspaceCustomizationSessionResult result;
    result.succeeded = true;

    const EditorNotification notification =
        MakeWorkspaceCustomizationNotification(
            WorkspaceCustomizationFeedbackOperation::Export,
            result,
            false);

    EXPECT_EQ(notification.severity, EditorNotificationSeverity::Success);
    EXPECT_EQ(notification.message, "Workspace customization exported.");
}

TEST(WorkspaceCustomizationFeedbackTests, FailureMessageCarriesFirstDiagnosticDetail)
{
    WorkspaceCustomizationSessionResult result;
    result.succeeded = false;
    result.diagnostics.push_back({
        EditorCustomizationDiagnosticSeverity::Error,
        "Customization.WriteFailed",
        "Could not write overlay.",
        {},
        {},
    });

    const EditorNotification notification =
        MakeWorkspaceCustomizationNotification(
            WorkspaceCustomizationFeedbackOperation::Reset,
            result,
            true);

    EXPECT_EQ(notification.severity, EditorNotificationSeverity::Error);
    EXPECT_EQ(notification.message, "Workspace customization reset failed.");
    EXPECT_EQ(notification.detail,
              "Customization.WriteFailed: Could not write overlay.");
}

TEST(ShortcutCustomizationControllerTests, NoSnapshotProducesUnconfiguredState)
{
    ShortcutCustomizationControllerConfig config;
    config.availability = MakeAvailability();

    ShortcutCustomizationController controller(config);

    EXPECT_EQ(controller.GetModel().status,
              ShortcutCustomizationStatus::Unconfigured);
    EXPECT_TRUE(controller.GetModel().actions.empty());
    EXPECT_TRUE(HasCode(controller.GetModel().diagnostics,
                        "Customization.NoCompositionSnapshot"));
}

TEST(ShortcutCustomizationControllerTests, SafeActionCanBeRebound)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    ShortcutCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ShortcutCustomizationController controller(config);

    ASSERT_TRUE(controller.SetShortcut("saga.command.file.save", "Ctrl+Alt+S"));

    const ShortcutCustomizationModel& model = controller.GetModel();
    EXPECT_TRUE(model.dirty);
    EXPECT_TRUE(model.restartRequired);
    const ShortcutCustomizationActionState* save =
        FindShortcutAction(model, "saga.command.file.save");
    ASSERT_NE(save, nullptr);
    ASSERT_TRUE(save->pendingChord.has_value());
    EXPECT_EQ(*save->pendingChord, "Ctrl+Alt+S");

    EditorCustomizationOverlay overlay = controller.BuildOverlayDelta();
    ASSERT_EQ(overlay.shortcutOverrides.size(), 1u);
    EXPECT_EQ(overlay.shortcutOverrides[0].actionId, "saga.command.file.save");
    EXPECT_EQ(overlay.shortcutOverrides[0].chord, "Ctrl+Alt+S");
    EXPECT_EQ(overlay.baseCompositionId, "saga.editor.default");
}

TEST(ShortcutCustomizationControllerTests, RejectsInvalidAndCollidingChords)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    ShortcutCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ShortcutCustomizationController controller(config);

    EXPECT_FALSE(controller.SetShortcut("saga.command.file.save", "Ctrl+"));
    EXPECT_TRUE(HasCode(controller.GetModel().diagnostics,
                        "Customization.InvalidShortcutChord"));

    EXPECT_FALSE(controller.SetShortcut("saga.command.file.save", "Ctrl+Z"));
    EXPECT_TRUE(HasCode(controller.GetModel().diagnostics,
                        "Customization.ShortcutCollision"));
}

TEST(ShortcutCustomizationControllerTests, LockedActionCannotBeRebound)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    ShortcutCustomizationControllerConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ShortcutCustomizationController controller(config);

    EXPECT_FALSE(controller.SetShortcut("saga.action.internal.mutate",
                                        "Ctrl+M"));
    EXPECT_TRUE(HasCode(controller.GetModel().diagnostics,
                        "Customization.InternalOnlyAction"));
}

TEST(ShortcutCustomizationSessionTests, SaveAndResetPreserveWorkspaceOverrides)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_shortcut_customization_session";
    fs::remove_all(root);
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();

    ShortcutCustomizationSession session;
    ShortcutCustomizationSessionConfig config;
    config.workspaceRoot = root;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    EditorCustomizationOverlay existing;
    existing.schemaVersion = kCurrentEditorCustomizationOverlayVersion;
    existing.baseCompositionId = "saga.editor.default";
    existing.baseArtifactHash = "artifact-hash";
    existing.overlayId = "user.workspace.default";
    existing.visibilityOverrides.push_back({ "saga.panel.console", true });
    ASSERT_TRUE(WorkspaceCustomizationOverlayStore()
                    .Save(session.OverlayPath(), existing)
                    .succeeded);
    ASSERT_TRUE(session.ReloadOverlay().succeeded);

    ASSERT_TRUE(session.GetController()->SetShortcut(
        "saga.command.file.save",
        "Ctrl+Alt+S"));
    ASSERT_TRUE(session.SaveOverlay().succeeded);

    WorkspaceCustomizationOverlayLoadResult loadResult =
        WorkspaceCustomizationOverlayStore().Load(session.OverlayPath());
    ASSERT_TRUE(loadResult.overlay.has_value());
    ASSERT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    ASSERT_EQ(loadResult.overlay->shortcutOverrides.size(), 1u);

    ASSERT_TRUE(session.ResetShortcuts().succeeded);
    loadResult = WorkspaceCustomizationOverlayStore().Load(session.OverlayPath());
    ASSERT_TRUE(loadResult.overlay.has_value());
    EXPECT_EQ(loadResult.overlay->visibilityOverrides.size(), 1u);
    EXPECT_TRUE(loadResult.overlay->shortcutOverrides.empty());
}

TEST(ShortcutCustomizationPanelViewModelTests, ListsRowsAndUpdatesPendingState)
{
    const ResolvedEditorCompositionSnapshot snapshot = MakeSnapshot();
    ShortcutCustomizationSession session;
    ShortcutCustomizationSessionConfig config;
    config.snapshot = &snapshot;
    config.availability = MakeAvailability();
    ASSERT_TRUE(session.Init(std::move(config)));

    ShortcutCustomizationPanelViewModel viewModel(session);
    ShortcutCustomizationPanelViewState state = viewModel.GetState();
    const ShortcutCustomizationActionRow* save =
        FindShortcutViewRow(state, "saga.command.file.save");
    ASSERT_NE(save, nullptr);
    EXPECT_TRUE(save->editable);
    EXPECT_EQ(save->currentChord, "Ctrl+S");

    ASSERT_TRUE(viewModel.SetShortcut("saga.command.file.save",
                                      "Ctrl+Alt+S"));
    state = viewModel.GetState();
    save = FindShortcutViewRow(state, "saga.command.file.save");
    ASSERT_NE(save, nullptr);
    EXPECT_TRUE(save->pending);
    EXPECT_EQ(save->effectiveChord, "Ctrl+Alt+S");
    EXPECT_TRUE(state.restartRequired);
}

TEST(ShortcutCustomizationFeedbackTests, SuccessMessageIncludesRestartPolicy)
{
    ShortcutCustomizationSessionResult result;
    result.succeeded = true;

    const EditorNotification notification =
        MakeShortcutCustomizationNotification(
            ShortcutCustomizationFeedbackOperation::Save,
            result,
            true);

    EXPECT_EQ(notification.source, kShortcutCustomizationNotificationSource);
    EXPECT_EQ(notification.severity, EditorNotificationSeverity::Success);
    EXPECT_EQ(notification.message,
              "Shortcut preferences saved. Restart editor to apply shortcut changes.");
}

TEST(ShortcutCustomizationFeedbackTests, FailureMessageCarriesFirstDiagnosticDetail)
{
    ShortcutCustomizationSessionResult result;
    result.succeeded = false;
    result.diagnostics.push_back({
        EditorCustomizationDiagnosticSeverity::Error,
        "Customization.ShortcutCollision",
        "Shortcut chord is already assigned.",
        {},
        {},
    });

    const EditorNotification notification =
        MakeShortcutCustomizationNotification(
            ShortcutCustomizationFeedbackOperation::Import,
            result,
            false);

    EXPECT_EQ(notification.severity, EditorNotificationSeverity::Error);
    EXPECT_EQ(notification.message, "Shortcut preferences imported failed.");
    EXPECT_EQ(notification.detail,
              "Customization.ShortcutCollision: Shortcut chord is already assigned.");
}

} // namespace
