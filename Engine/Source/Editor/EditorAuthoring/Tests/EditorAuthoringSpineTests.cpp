#include "SagaEditor/Authoring/DiagnosticsPanelView.h"
#include "SagaEditor/Authoring/EditorShellCustomizationReport.h"
#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"
#include "SagaEditor/Authoring/SceneAssetBrowserInventoryView.h"
#include "SagaEditor/Authoring/ScriptArtifactIndex.h"
#include "SagaEditor/Authoring/ScriptBehaviorInspectorView.h"
#include "SagaEditor/Authoring/ScriptBehaviorProjectionView.h"
#include "SagaEditor/Authoring/ScriptPatchEvaluationView.h"
#include "SagaEditor/Authoring/ScriptPatchReviewWorkflowView.h"
#include "SagaEditor/Authoring/ProjectReadinessView.h"
#include "SagaEditor/Authoring/ViewNavigationWorkflowState.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{

namespace fs = std::filesystem;
using namespace SagaEditor::Authoring;

constexpr const char* kEmptySha256 =
    "e3b0c44298fc1c149afbf4c8996fb924"
    "27ae41e4649b934ca495991b7852b855";

[[nodiscard]] fs::path MakeTempRoot(const std::string& name)
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() /
        ("saga_editor_authoring_" + name + "_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc | std::ios::binary);
    output << text;
}

[[nodiscard]] std::string ReadFile(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

[[nodiscard]] bool HasVisiblePanel(
    const EditorShellCustomizationReport& report,
    const std::string& panelId)
{
    return std::any_of(report.visiblePanels.begin(),
                       report.visiblePanels.end(),
                       [&](const EditorShellPanelVisibility& panel)
                       {
                           return panel.id == panelId && panel.visible;
                       });
}

[[nodiscard]] bool HasWorkflowVisibility(
    const EditorShellCustomizationReport& report,
    const std::string& workflowId,
    bool visible)
{
    return std::any_of(report.workflowVisibility.begin(),
                       report.workflowVisibility.end(),
                       [&](const EditorShellWorkflowVisibility& workflow)
                       {
                           return workflow.id == workflowId &&
                               workflow.visible == visible &&
                               workflow.available;
                       });
}

[[nodiscard]] fs::path WriteProject(const fs::path& root)
{
    const fs::path manifest = root / "MultiplayerSandbox.sagaproj";
    WriteFile(manifest, R"({
  "schemaVersion": 0,
  "projectId": "multiplayer-sandbox",
  "displayName": "MultiplayerSandbox",
  "paths": {
    "diagnostics": "Diagnostics",
    "generatedReports": "Build/Reports"
  },
  "scriptFolders": [
    { "id": "scripts", "path": "Scripts" }
  ],
  "launchProfiles": [
    { "id": "local-server-headless", "path": "launch_profiles.json" }
  ],
  "packageProfiles": [
    { "id": "server", "path": "package_profiles.json" }
  ],
  "unknownFullValidationField": { "ownedBy": "SagaProjectKit" }
})");
    return manifest;
}

void WriteCoreArtifacts(const fs::path& root)
{
    WriteFile(root / "Scripts" / "DoorLogic.High.cs", "");
    WriteFile(root / "Build" / "SagaScript" / "source_map.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "source-map",
  "status": "Passed",
  "sourceHash": ")" + std::string(kEmptySha256) + R"(",
  "files": [
    {
      "sourceFile": "Scripts/DoorLogic.High.cs",
      "sourceHash": ")" + std::string(kEmptySha256) + R"(",
      "byteLength": 0
    }
  ],
  "behaviors": [
    {
      "behaviorId": "behavior://sandbox/door-interact",
      "apiLevel": "High",
      "apiDomain": "Gameplay",
      "sourceFile": "Scripts/DoorLogic.High.cs",
      "sourceHash": ")" + std::string(kEmptySha256) + R"(",
      "sourceSpan": {
        "startLine": 1,
        "startColumn": 1,
        "endLine": 1,
        "endColumn": 1,
        "startByte": 0,
        "endByte": 0
      },
      "nodes": [
        {
          "nodeId": "node.high.call",
          "kind": "Call",
          "displayName": "Door.Open",
          "apiLevel": "High",
          "apiDomain": "Gameplay",
          "capability": "ProjectionOnly",
          "projectionCompatibility": "ReadOnly",
          "readOnly": true,
          "sourceSpan": {
            "startLine": 1,
            "startColumn": 1,
            "endLine": 1,
            "endColumn": 1,
            "startByte": 0,
            "endByte": 0
          }
        }
      ]
    },
    {
      "behaviorId": "behavior://sandbox/door-state",
      "apiLevel": "Low",
      "apiDomain": "Gameplay",
      "sourceFile": "Scripts/DoorLogic.High.cs",
      "sourceHash": ")" + std::string(kEmptySha256) + R"(",
      "sourceSpan": {
        "startLine": 1,
        "startColumn": 1,
        "endLine": 1,
        "endColumn": 1,
        "startByte": 0,
        "endByte": 0
      },
      "nodes": [
        {
          "nodeId": "node.low.opaque",
          "kind": "Opaque",
          "displayName": "Unsupported region",
          "apiLevel": "Low",
          "apiDomain": "Gameplay",
          "capability": "Deferred",
          "projectionCompatibility": "Deferred",
          "readOnly": true,
          "opaqueReason": "LINQ expression is opaque",
          "sourceSpan": {
            "startLine": 1,
            "startColumn": 1,
            "endLine": 1,
            "endColumn": 1,
            "startByte": 0,
            "endByte": 0
          }
        }
      ]
    }
  ],
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    WriteFile(root / "Build" / "SagaScript" / "projection_report.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "project-blocks",
  "status": "Passed",
  "apiLevel": "Mixed",
  "apiDomain": "Gameplay",
  "sourceHash": ")" + std::string(kEmptySha256) + R"(",
  "behaviors": [
    {
      "behaviorId": "behavior://sandbox/door-interact",
      "apiLevel": "High",
      "apiDomain": "Gameplay",
      "compatibility": "Supported",
      "sourceFile": "Scripts/DoorLogic.High.cs",
      "sourceHash": ")" + std::string(kEmptySha256) + R"(",
      "nodes": [
        {
          "nodeId": "node.high.call",
          "kind": "Call",
          "displayName": "Door.Open",
          "apiLevel": "High",
          "apiDomain": "Gameplay",
          "capability": "ProjectionOnly",
          "projectionCompatibility": "ReadOnly",
          "readOnly": true
        }
      ]
    },
    {
      "behaviorId": "behavior://sandbox/door-state",
      "apiLevel": "Low",
      "apiDomain": "Gameplay",
      "compatibility": "Unsupported",
      "sourceFile": "Scripts/DoorLogic.High.cs",
      "sourceHash": ")" + std::string(kEmptySha256) + R"(",
      "nodes": [
        {
          "nodeId": "node.low.opaque",
          "kind": "Opaque",
          "displayName": "Unsupported region",
          "apiLevel": "Low",
          "apiDomain": "Gameplay",
          "capability": "Deferred",
          "projectionCompatibility": "Deferred",
          "readOnly": true,
          "opaqueReason": "LINQ expression is opaque"
        }
      ]
    }
  ],
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    WriteFile(root / "Build" / "SagaScript" / "analysis_report.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "analyze",
  "status": "Passed",
  "sourceHash": ")" + std::string(kEmptySha256) + R"(",
  "behaviors": [],
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    WriteFile(root / "Build" / "SagaScript" / "runtime_bindings.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "emit-bindings",
  "status": "Passed",
  "sourceHash": ")" + std::string(kEmptySha256) + R"(",
  "bindings": [
    {
      "behaviorId": "behavior://sandbox/door-interact",
      "apiLevel": "High",
      "apiDomain": "Gameplay",
      "compatibility": "Supported",
      "nodes": [
        {
          "nodeId": "node.high.call",
          "kind": "Call",
          "apiDomain": "Gameplay",
          "apiLevel": "High",
          "capability": "ProjectionOnly",
          "projectionCompatibility": "ReadOnly",
          "compatibilityClassification": "ProjectionOnly",
          "runtimeProof": "ProjectionOnly"
        }
      ]
    }
  ],
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    WriteFile(root / "Build" / "SagaScript" / "node_metadata.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "project-blocks",
  "status": "Passed",
  "sourceHash": ")" + std::string(kEmptySha256) + R"(",
  "nodes": [
    {
      "nodeId": "node.high.call",
      "displayName": "Door.Open",
      "apiDomain": "Gameplay",
      "apiLevel": "High",
      "capability": "ProjectionOnly",
      "projectionCompatibility": "ReadOnly",
      "readOnly": true
    },
    {
      "nodeId": "node.low.opaque",
      "displayName": "Unsupported region",
      "apiDomain": "Gameplay",
      "apiLevel": "Low",
      "capability": "Deferred",
      "projectionCompatibility": "Deferred",
      "readOnly": true
    }
  ],
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    WriteFile(root / "Build" / "SagaScript" /
        "csharp_compatibility_profile_v2.json", R"({
  "schemaVersion": 2,
  "tool": "sagascript",
  "command": "compatibility-profile",
  "status": "Passed",
  "constructs": [
    {
      "constructId": "node.high.call",
      "kind": "Call",
      "classification": "ProjectionOnly",
      "apiDomain": "Gameplay",
      "apiLevel": "High",
      "capability": "ProjectionOnly"
    },
    {
      "constructId": "node.low.opaque",
      "kind": "Opaque",
      "classification": "Deferred",
      "apiDomain": "Gameplay",
      "apiLevel": "Low",
      "capability": "Deferred"
    }
  ],
  "summary": { "ProjectionOnly": 1, "Deferred": 1 },
  "diagnostics": []
})");
}

} // namespace

TEST(EditorAuthoringSpineTests, LoadsProjectReadinessProjectSubsetReadOnly)
{
    const fs::path root = MakeTempRoot("project_subset");
    const fs::path manifest = WriteProject(root);

    const auto before = fs::last_write_time(manifest);
    ProjectReadinessView view = LoadProjectReadinessView(manifest);

    ASSERT_TRUE(view.ok);
    EXPECT_EQ(view.projectId, "multiplayer-sandbox");
    EXPECT_EQ(view.displayName, "MultiplayerSandbox");
    ASSERT_EQ(view.scriptFolders.size(), 1u);
    EXPECT_EQ(view.scriptFolders[0].path, root / "Scripts");
    ASSERT_EQ(view.launchProfiles.size(), 1u);
    ASSERT_EQ(view.packageProfiles.size(), 1u);
    EXPECT_EQ(fs::last_write_time(manifest), before);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, InvalidProjectSubsetReportsEditorParseFailure)
{
    const fs::path root = MakeTempRoot("invalid_project");
    const fs::path manifest = root / "Broken.sagaproj";
    WriteFile(manifest, "{ invalid json");

    ProjectReadinessView view = LoadProjectReadinessView(manifest);

    EXPECT_FALSE(view.ok);
    ASSERT_FALSE(view.diagnostics.empty());
    EXPECT_EQ(view.diagnostics[0].code,
              "Editor.Project.EditorSubsetParseFailed");
    EXPECT_NE(view.diagnostics[0].message.find("editor subset parse failed"),
              std::string::npos);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ArtifactIndexUsesHashFreshnessNotMtime)
{
    const fs::path root = MakeTempRoot("artifact_index");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);

    ProjectReadinessView view = LoadProjectReadinessView(manifest);
    ScriptArtifactIndex index = BuildScriptArtifactIndex(view);

    EXPECT_EQ(index.overallStatus, ScriptArtifactStatus::Ready);
    const auto sourceMapIt = std::find_if(
        index.artifacts.begin(),
        index.artifacts.end(),
        [](const ScriptArtifactEntry& entry)
        {
            return entry.artifactName == "sourceMap";
        });
    ASSERT_NE(sourceMapIt, index.artifacts.end());
    EXPECT_EQ(sourceMapIt->status, ScriptArtifactStatus::Ready);
    ASSERT_EQ(sourceMapIt->sources.size(), 1u);
    EXPECT_EQ(sourceMapIt->sources[0].status, ScriptArtifactStatus::Ready);

    WriteFile(root / "Scripts" / "DoorLogic.High.cs", "changed");
    ScriptArtifactIndex staleIndex = BuildScriptArtifactIndex(view);
    const auto staleSourceMapIt = std::find_if(
        staleIndex.artifacts.begin(),
        staleIndex.artifacts.end(),
        [](const ScriptArtifactEntry& entry)
        {
            return entry.artifactName == "sourceMap";
        });
    ASSERT_NE(staleSourceMapIt, staleIndex.artifacts.end());
    EXPECT_EQ(staleSourceMapIt->status, ScriptArtifactStatus::Stale);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ArtifactIndexReportsMissingSourceAndUnknownFreshness)
{
    const fs::path root = MakeTempRoot("artifact_statuses");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);
    fs::remove(root / "Scripts" / "DoorLogic.High.cs");

    ProjectReadinessView view = LoadProjectReadinessView(manifest);
    ScriptArtifactIndex missingSourceIndex = BuildScriptArtifactIndex(view);
    const auto sourceMapIt = std::find_if(
        missingSourceIndex.artifacts.begin(),
        missingSourceIndex.artifacts.end(),
        [](const ScriptArtifactEntry& entry)
        {
            return entry.artifactName == "sourceMap";
        });
    ASSERT_NE(sourceMapIt, missingSourceIndex.artifacts.end());
    EXPECT_EQ(sourceMapIt->status, ScriptArtifactStatus::MissingSource);

    WriteFile(root / "Build" / "SagaScript" / "source_map.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "source-map",
  "status": "Passed",
  "files": [
    { "sourceFile": "Scripts/DoorLogic.High.cs" }
  ],
  "behaviors": []
})");
    ScriptArtifactIndex unknownIndex = BuildScriptArtifactIndex(view);
    const auto unknownSourceMapIt = std::find_if(
        unknownIndex.artifacts.begin(),
        unknownIndex.artifacts.end(),
        [](const ScriptArtifactEntry& entry)
        {
            return entry.artifactName == "sourceMap";
        });
    ASSERT_NE(unknownSourceMapIt, unknownIndex.artifacts.end());
    EXPECT_EQ(unknownSourceMapIt->status,
              ScriptArtifactStatus::UnknownFreshness);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ProjectionViewIsReadOnlyAndPreservesAxes)
{
    const fs::path root = MakeTempRoot("projection");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptBehaviorProjectionLoadResult result =
        LoadScriptBehaviorProjectionViews(project);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.behaviors.size(), 2u);
    EXPECT_EQ(result.behaviors[0].apiDomain, "Gameplay");
    EXPECT_EQ(result.behaviors[0].apiLevel, "High");
    EXPECT_EQ(result.behaviors[1].apiLevel, "Low");
    ASSERT_EQ(result.behaviors[1].nodes.size(), 1u);
    EXPECT_EQ(result.behaviors[1].nodes[0].kind, "Opaque");
    EXPECT_TRUE(result.behaviors[1].nodes[0].readOnly);
    EXPECT_EQ(result.behaviors[1].nodes[0].opaqueReason,
              "LINQ expression is opaque");
    EXPECT_EQ(result.behaviors[0].sourceLink.freshness,
              ScriptArtifactStatus::Ready);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, PatchEvaluationReviewNeverExposesApply)
{
    const fs::path root = MakeTempRoot("patch_evaluation");
    const fs::path manifest = WriteProject(root);
    WriteFile(root / "Build" / "SagaScript" / "patch_evaluation.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "patch-evaluation",
  "status": "Passed",
  "operation": "ReplaceStringLiteral",
  "nodeId": "node.high.literal",
  "sourceFile": "Scripts/DoorLogic.High.cs",
  "baseSourceHash": ")" + std::string(kEmptySha256) + R"(",
  "mutatesSource": false,
  "evaluation": {
    "startByte": 12,
    "endByte": 17,
    "oldText": "\"key\"",
    "newText": "\"gold_key\""
  },
  "summary": { "hasBlockingDiagnostics": false },
  "diagnostics": []
})");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptPatchEvaluationView evaluation = LoadScriptPatchEvaluationView(project);

    ASSERT_TRUE(evaluation.ok);
    EXPECT_FALSE(evaluation.mutatesSource);
    EXPECT_FALSE(evaluation.applyAvailable);
    EXPECT_EQ(evaluation.operation, "ReplaceStringLiteral");
    EXPECT_EQ(evaluation.oldText, "\"key\"");
    EXPECT_EQ(evaluation.newText, "\"gold_key\"");

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, InspectorShowsBindingsAndUnsupportedReasons)
{
    const fs::path root = MakeTempRoot("inspector");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptBehaviorInspectorLoadResult result =
        LoadScriptBehaviorInspectorViews(project);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.behaviors.size(), 2u);
    EXPECT_TRUE(result.behaviors[0].hasRuntimeBinding);
    EXPECT_EQ(result.behaviors[0].bindingStatus, "Ready");
    EXPECT_FALSE(result.behaviors[1].hasRuntimeBinding);
    EXPECT_EQ(result.behaviors[1].bindingStatus, "Missing");
    ASSERT_EQ(result.behaviors[1].unsupportedReasons.size(), 1u);
    EXPECT_EQ(result.behaviors[1].unsupportedReasons[0],
              "LINQ expression is opaque");

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ProjectBrowserReportsMissingManifestDeterministically)
{
    const fs::path root = MakeTempRoot("missing_manifest");

    ProjectBrowserWorkflowView browser =
        LoadProjectBrowserWorkflowView(root / "Missing.sagaproj");

    EXPECT_FALSE(browser.ok);
    ASSERT_FALSE(browser.diagnostics.empty());
    EXPECT_EQ(browser.diagnostics[0].code,
              "Editor.Project.EditorSubsetParseFailed");

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ProjectBrowserListsWorkflowSectionsReadOnly)
{
    const fs::path root = MakeTempRoot("project_browser");
    const fs::path manifest = WriteProject(root);
    WriteFile(root / "Scripts" / "DoorLogic.High.cs", "");
    WriteFile(root / "Assets" / "README.md", "asset root");
    WriteFile(root / "Scenes" / "README.md", "scene root");
    WriteFile(root / "Build" / "Reports" / "README.md", "reports root");
    WriteFile(root / "Diagnostics" / "README.md", "diagnostics root");
    WriteFile(root / "launch_profiles.json", "{}");
    WriteFile(root / "package_profiles.json", "{}");

    const auto before = fs::last_write_time(manifest);
    ProjectBrowserWorkflowView browser = LoadProjectBrowserWorkflowView(manifest);

    EXPECT_EQ(browser.projectId, "multiplayer-sandbox");
    EXPECT_EQ(browser.displayName, "MultiplayerSandbox");
    EXPECT_EQ(browser.sections.size(), 5u);
    EXPECT_NE(std::find_if(browser.sections.begin(),
                           browser.sections.end(),
                           [](const ProjectBrowserSectionView& section)
                           {
                               return section.name == "Scripts" &&
                                   section.exists;
                           }),
              browser.sections.end());
    EXPECT_NE(std::find_if(browser.sections.begin(),
                           browser.sections.end(),
                           [](const ProjectBrowserSectionView& section)
                           {
                               return section.name == "Assets" &&
                                   section.exists;
                           }),
              browser.sections.end());
    ASSERT_EQ(browser.launchProfiles.size(), 1u);
    ASSERT_EQ(browser.packageProfiles.size(), 1u);
    EXPECT_EQ(fs::last_write_time(manifest), before);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, StarterArenaLoadsMinimumEditorShellWorkflowReadOnly)
{
    const fs::path manifest = fs::path(SAGA_SOURCE_ROOT) /
        "Samples" / "StarterArena" / "StarterArena.sagaproj";

    ProjectReadinessView project =
        LoadProjectReadinessView(manifest);
    ProjectBrowserWorkflowView browser =
        LoadProjectBrowserWorkflowView(manifest);

    ASSERT_TRUE(project.ok);
    EXPECT_EQ(project.projectId, "starter-arena");
    EXPECT_EQ(project.displayName, "StarterArena");
    EXPECT_EQ(project.manifestPath.filename(), "StarterArena.sagaproj");
    EXPECT_EQ(project.diagnosticsPath.filename(), "Diagnostics");
    EXPECT_EQ(project.generatedReportsPath.filename(), "Reports");
    ASSERT_EQ(project.scriptFolders.size(), 1u);
    EXPECT_EQ(project.scriptFolders[0].id, "starter-arena.scripts");
    EXPECT_TRUE(fs::exists(project.scriptFolders[0].path));
    EXPECT_TRUE(project.launchProfiles.empty());
    EXPECT_TRUE(project.packageProfiles.empty());

    ASSERT_EQ(browser.sections.size(), 5u);
    EXPECT_NE(std::find_if(browser.sections.begin(),
                           browser.sections.end(),
                           [](const ProjectBrowserSectionView& section)
                           {
                               return section.name == "Scenes" &&
                                   section.exists &&
                                   section.status == "Ready";
                           }),
              browser.sections.end());
    EXPECT_NE(std::find_if(browser.sections.begin(),
                           browser.sections.end(),
                           [](const ProjectBrowserSectionView& section)
                           {
                               return section.name == "Scripts" &&
                                   section.exists &&
                                   section.status == "Ready";
                           }),
              browser.sections.end());
    EXPECT_FALSE(browser.ok);
    EXPECT_NE(std::find_if(browser.diagnostics.begin(),
                           browser.diagnostics.end(),
                           [](const AuthoringDiagnostic& diagnostic)
                           {
                               return diagnostic.code ==
                                   "Editor.ProjectBrowser.PackageProfilesMissing";
                           }),
              browser.diagnostics.end());
    EXPECT_NE(std::find_if(browser.diagnostics.begin(),
                           browser.diagnostics.end(),
                           [](const AuthoringDiagnostic& diagnostic)
                           {
                               return diagnostic.code ==
                                   "Editor.ProjectBrowser.AssetRootMissing";
                           }),
              browser.diagnostics.end());
}

TEST(EditorAuthoringSpineTests, StarterArenaCustomizationReportUsesProjectReadinessDefault)
{
    const fs::path manifest = fs::path(SAGA_SOURCE_ROOT) /
        "Samples" / "StarterArena" / "StarterArena.sagaproj";
    const auto beforeWriteTime = fs::last_write_time(manifest);

    EditorShellCustomizationReport report =
        BuildEditorShellCustomizationReport("");

    EXPECT_EQ(fs::last_write_time(manifest), beforeWriteTime);
    EXPECT_EQ(report.status, "Ready");
    EXPECT_EQ(report.requestedProfileId, "project_readiness");
    EXPECT_EQ(report.resolvedProfileId, "saga.profile.basic");
    EXPECT_EQ(report.viewPresetId, "project_readiness");
    EXPECT_EQ(report.layoutPresetId, "basic");
    EXPECT_EQ(report.shortcutMapId, "saga.shortcuts.basic");
    EXPECT_TRUE(report.personalPreferenceOnly);
    EXPECT_FALSE(report.sharedProjectTruthMutable);
    EXPECT_EQ(report.profileSource, "builtin-report-metadata");
    EXPECT_TRUE(HasVisiblePanel(report, "saga.panel.production_dashboard"));
    EXPECT_TRUE(HasWorkflowVisibility(report, "validate-project", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "runtime-smoke", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "server-smoke", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "package-preflight", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "visual-blocks-cli-chain", true));
    EXPECT_TRUE(report.capabilities.canViewProject);
    EXPECT_TRUE(report.capabilities.canViewDiagnostics);
    EXPECT_TRUE(report.capabilities.canViewScriptProjection);
    EXPECT_TRUE(report.capabilities.canRunWorkflowCommandReference);
    EXPECT_FALSE(report.capabilities.canMutateProjectManifest);
    EXPECT_FALSE(report.capabilities.canEditCSharpInPlace);
    EXPECT_FALSE(report.capabilities.canUseVisualBlocksEditorUi);
    EXPECT_FALSE(report.capabilities.canConfigureCollaborationServer);
    EXPECT_FALSE(report.capabilities.canBuildDistribution);
}

TEST(EditorAuthoringSpineTests, StarterArenaCustomizationReportUsesScriptAuthoringAlias)
{
    const fs::path manifest = fs::path(SAGA_SOURCE_ROOT) /
        "Samples" / "StarterArena" / "StarterArena.sagaproj";
    const auto beforeWriteTime = fs::last_write_time(manifest);

    EditorShellCustomizationReport report =
        BuildEditorShellCustomizationReport("script_authoring");

    EXPECT_EQ(fs::last_write_time(manifest), beforeWriteTime);
    EXPECT_EQ(report.status, "Ready");
    EXPECT_EQ(report.requestedProfileId, "script_authoring");
    EXPECT_EQ(report.resolvedProfileId, "saga.profile.node_editor");
    EXPECT_EQ(report.viewPresetId, "script_authoring");
    EXPECT_EQ(report.layoutPresetId, "node_editor");
    EXPECT_EQ(report.shortcutMapId, "saga.shortcuts.node_editor");
    EXPECT_TRUE(HasVisiblePanel(report, "saga.panel.graph"));
    EXPECT_TRUE(HasWorkflowVisibility(report, "validate-project", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "sagascript-analyze", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "sagascript-compile", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "visual-blocks-cli-chain", true));
    EXPECT_TRUE(HasWorkflowVisibility(report, "runtime-smoke", false));
    EXPECT_TRUE(HasWorkflowVisibility(report, "server-smoke", false));
    EXPECT_TRUE(report.capabilities.canViewProject);
    EXPECT_TRUE(report.capabilities.canViewDiagnostics);
    EXPECT_TRUE(report.capabilities.canViewScriptProjection);
    EXPECT_FALSE(report.capabilities.canMutateProjectManifest);
    EXPECT_FALSE(report.capabilities.canEditCSharpInPlace);
    EXPECT_FALSE(report.capabilities.canUseVisualBlocksEditorUi);
    EXPECT_FALSE(report.capabilities.canConfigureCollaborationServer);
    EXPECT_FALSE(report.capabilities.canBuildDistribution);
}

TEST(EditorAuthoringSpineTests, CustomizationReportUnknownProfileFallsBackReadOnly)
{
    EditorShellCustomizationReport report =
        BuildEditorShellCustomizationReport("missing_profile");

    EXPECT_EQ(report.status, "UnknownProfile");
    EXPECT_EQ(report.requestedProfileId, "missing_profile");
    EXPECT_EQ(report.resolvedProfileId, "saga.profile.basic");
    EXPECT_EQ(report.viewPresetId, "project_readiness");
    ASSERT_EQ(report.diagnostics.size(), 1u);
    EXPECT_NE(report.diagnostics[0].find("missing_profile"), std::string::npos);
    EXPECT_FALSE(report.capabilities.canMutateProjectManifest);
    EXPECT_FALSE(report.capabilities.canEditCSharpInPlace);
    EXPECT_FALSE(report.capabilities.canUseVisualBlocksEditorUi);
    EXPECT_FALSE(report.capabilities.canConfigureCollaborationServer);
    EXPECT_FALSE(report.capabilities.canBuildDistribution);
}

TEST(EditorAuthoringSpineTests, BehaviorInspectorConsumesAllScriptEvidence)
{
    const fs::path root = MakeTempRoot("inspector_evidence");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);
    WriteFile(root / "Build" / "SagaScript" /
        "script_artifact_validation_report.json", R"({
  "schemaVersion": 1,
  "tool": "sagascript",
  "command": "validate-artifacts",
  "status": "Failed",
  "runtimeProofSummary": {
    "runtimeBackedWithEvidence": 0,
    "runtimeBackedMissingEvidence": 1,
    "projectionOnly": 1,
    "deferred": 1
  },
  "diagnostics": [
    {
      "severity": "Warning",
      "code": "Script.Artifacts.RuntimeEvidenceMissing",
      "message": "runtime-backed node is missing evidence",
      "sourceFile": "Build/SagaScript/runtime_bindings.json"
    }
  ]
})");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptBehaviorInspectorLoadResult result =
        LoadScriptBehaviorInspectorViews(project);

    ASSERT_EQ(result.behaviors.size(), 2u);
    ASSERT_EQ(result.behaviors[0].nodes.size(), 1u);
    EXPECT_EQ(result.behaviors[0].nodes[0].capability, "ProjectionOnly");
    EXPECT_EQ(result.behaviors[0].nodes[0].runtimeProofState,
              "ProjectionOnly");
    EXPECT_EQ(result.behaviors[0].nodes[0].nodeMetadataStatus, "Ready");
    ASSERT_EQ(result.behaviors[1].nodes.size(), 1u);
    EXPECT_EQ(result.behaviors[1].nodes[0].capability, "Deferred");
    EXPECT_EQ(result.behaviors[1].nodes[0].runtimeProofState,
              "NotRuntimeProof");
    EXPECT_FALSE(result.diagnostics.empty());

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, BehaviorInspectorBlocksRuntimeBackedMissingEvidence)
{
    const fs::path root = MakeTempRoot("runtime_evidence_missing");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);
    WriteFile(root / "Build" / "SagaScript" / "node_metadata.json", R"({
  "schemaVersion": 1,
  "status": "Passed",
  "nodes": [
    {
      "nodeId": "node.high.call",
      "displayName": "Door.Open",
      "apiDomain": "Gameplay",
      "apiLevel": "High",
      "capability": "RuntimeBacked",
      "projectionCompatibility": "ReadOnly",
      "readOnly": true
    }
  ],
  "diagnostics": []
})");
    WriteFile(root / "Build" / "SagaScript" / "runtime_bindings.json", R"({
  "schemaVersion": 1,
  "status": "Passed",
  "bindings": [
    {
      "behaviorId": "behavior://sandbox/door-interact",
      "nodes": [
        {
          "nodeId": "node.high.call",
          "capability": "RuntimeBacked",
          "runtimeProof": "RuntimeBackedWithEvidenceMissing",
          "compatibilityClassification": "RuntimeBacked"
        }
      ]
    }
  ],
  "diagnostics": []
})");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptBehaviorInspectorLoadResult result =
        LoadScriptBehaviorInspectorViews(project);

    ASSERT_FALSE(result.behaviors.empty());
    ASSERT_FALSE(result.behaviors[0].nodes.empty());
    EXPECT_EQ(result.behaviors[0].nodes[0].runtimeProofState,
              "RuntimeBackedWithEvidenceMissing");

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, DiagnosticsPanelGroupsSummaryAndMissingReport)
{
    const fs::path root = MakeTempRoot("diagnostics_panel");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);
    WriteFile(root / "Scripts" / "DoorLogic.High.cs", "");
    WriteFile(root / "Assets" / "README.md", "asset root");
    WriteFile(root / "Scenes" / "README.md", "scene root");
    WriteFile(root / "Diagnostics" / "README.md", "diagnostics root");
    WriteFile(root / "launch_profiles.json", "{}");
    WriteFile(root / "package_profiles.json", "{}");
    WriteFile(root / "Build" / "Reports" / "diagnostics_summary.json", R"({
  "schemaVersion": 1,
  "tool": "diagnostic-check",
  "command": "summarize",
  "status": "Attention",
  "diagnostics": [
    {
      "severity": "Error",
      "code": "Runtime.Critical",
      "message": "critical diagnostic",
      "sourceFile": "Build/Reports/runtime_report.json"
    },
    {
      "severity": "Warning",
      "code": "Runtime.Warning",
      "message": "warning diagnostic",
      "sourceFile": "Build/Reports/runtime_report.json"
    },
    {
      "severity": "Info",
      "code": "Runtime.Info",
      "message": "info diagnostic",
      "sourceFile": "Build/Reports/runtime_report.json"
    }
  ]
})");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    DiagnosticsPanelView panel = LoadDiagnosticsPanelView(project);

    EXPECT_FALSE(panel.refresh.writesFiles);
    EXPECT_FALSE(panel.critical.entries.empty());
    EXPECT_FALSE(panel.warning.entries.empty());
    EXPECT_FALSE(panel.info.entries.empty());

    fs::remove(root / "Build" / "Reports" / "diagnostics_summary.json");
    DiagnosticsPanelView missing = LoadDiagnosticsPanelView(project);
    EXPECT_NE(std::find_if(missing.warning.entries.begin(),
                           missing.warning.entries.end(),
                           [](const DiagnosticsPanelEntryView& entry)
                           {
                               return entry.code ==
                                   "Editor.DiagnosticsPanel.SummaryMissing";
                           }),
              missing.warning.entries.end());

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, PatchReviewWorkflowDisplaysSagaScriptEvidenceOnly)
{
    const fs::path root = MakeTempRoot("patch_review_workflow");
    const fs::path manifest = WriteProject(root);
    WriteFile(root / "Scripts" / "DoorLogic.High.cs", "\"key\"");
    const std::string before = ReadFile(root / "Scripts" /
        "DoorLogic.High.cs");
    WriteFile(root / "Build" / "SagaScript" / "patch_evaluation.json", R"({
  "schemaVersion": 1,
  "status": "Passed",
  "operation": "ReplaceStringLiteral",
  "operationId": "op-1",
  "nodeId": "node.high.literal",
  "sourceFile": "Scripts/DoorLogic.High.cs",
  "baseSourceHash": ")" + std::string(kEmptySha256) + R"(",
  "mutatesSource": false,
  "evaluation": {
    "startByte": 0,
    "endByte": 5,
    "oldText": "\"key\"",
    "newText": "\"gold_key\""
  },
  "diagnostics": []
})");
    WriteFile(root / "Build" / "SagaScript" / "patch_diff_report.json", R"({
  "schemaVersion": 1,
  "command": "patch-diff",
  "status": "Passed",
  "operation": "ReplaceStringLiteral",
  "operationId": "op-1",
  "nodeId": "node.high.literal",
  "sourceFile": "Scripts/DoorLogic.High.cs",
  "baseSourceHash": ")" + std::string(kEmptySha256) + R"(",
  "proposedSourceHash": "proposed",
  "oldText": "\"key\"",
  "newText": "\"gold_key\"",
  "mutatesSource": false,
  "byteDiff": { "startByte": 0, "endByte": 5 },
  "diagnostics": []
})");
    WriteFile(root / "Build" / "SagaScript" / "patch_review_report.json", R"({
  "schemaVersion": 1,
  "command": "patch-review",
  "status": "Passed",
  "decision": "Approved",
  "operation": "ReplaceStringLiteral",
  "operationId": "op-1",
  "mutatesSource": false,
  "appliesPatch": false,
  "diagnostics": []
})");
    WriteFile(root / "Build" / "SagaScript" / "patch_apply_report.json", R"({
  "schemaVersion": 1,
  "command": "patch-apply",
  "status": "Passed",
  "operation": "ReplaceStringLiteral",
  "operationId": "op-1",
  "newSourceHash": "new",
  "rollbackStatus": "NotNeeded",
  "mutatesSource": true,
  "staleArtifacts": ["source_map.json"],
  "diagnostics": []
})");
    WriteFile(root / "Build" / "SagaScript" / "patch_rollback_report.json", R"({
  "schemaVersion": 1,
  "command": "patch-rollback",
  "status": "Passed",
  "operationId": "op-1",
  "rollbackStatus": "Restored",
  "mutatesSource": true,
  "diagnostics": []
})");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptPatchReviewWorkflowView review =
        LoadScriptPatchReviewWorkflowView(project);

    EXPECT_FALSE(review.applyAvailable);
    EXPECT_EQ(review.reviewDecision, "Approved");
    EXPECT_FALSE(review.reviewApprovedMeansApplied);
    EXPECT_EQ(review.operation, "ReplaceStringLiteral");
    EXPECT_EQ(review.rollbackStatus, "NotNeeded");
    EXPECT_FALSE(review.staleArtifacts.empty());
    ASSERT_FALSE(review.actions.empty());
    EXPECT_EQ(review.actions[0].disabledReason,
              "SagaScript-owned source mutation only.");
    const std::string after = ReadFile(root / "Scripts" /
        "DoorLogic.High.cs");
    EXPECT_EQ(after, before);

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, SceneAssetBrowserReportsMissingSourceTruth)
{
    const fs::path root = MakeTempRoot("scene_asset_inventory");
    const fs::path manifest = WriteProject(root);
    WriteFile(root / "Assets" / "README.md", "asset root");

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    SceneAssetBrowserInventoryView inventory =
        LoadSceneAssetBrowserInventoryView(project);

    EXPECT_EQ(inventory.sceneEntitySourceStatus, "MissingSourceOfTruth");
    EXPECT_NE(std::find_if(inventory.diagnostics.begin(),
                           inventory.diagnostics.end(),
                           [](const AuthoringDiagnostic& diagnostic)
                           {
                               return diagnostic.code ==
                                   "Editor.SceneAssetBrowser.AssetManifestMissing";
                           }),
              inventory.diagnostics.end());

    fs::remove_all(root);
}

TEST(EditorAuthoringSpineTests, ViewSwitchingPreservesSelectionAndReadOnlyPolicy)
{
    const fs::path root = MakeTempRoot("view_navigation");
    const fs::path manifest = WriteProject(root);
    WriteCoreArtifacts(root);

    ProjectReadinessView project = LoadProjectReadinessView(manifest);
    ScriptBehaviorInspectorLoadResult inspector =
        LoadScriptBehaviorInspectorViews(project);
    ASSERT_EQ(inspector.behaviors.size(), 2u);
    ASSERT_EQ(inspector.behaviors[1].nodes.size(), 1u);

    ViewNavigationSelectionState selection;
    selection.behaviorId = inspector.behaviors[1].behaviorId;
    selection.nodeId = inspector.behaviors[1].nodes[0].nodeId;
    selection.artifactPath = project.projectRoot / "Build" / "SagaScript" /
        "projection_report.json";
    selection.sourceLink = inspector.behaviors[1].nodes[0].sourceLink;

    ViewNavigationWorkflowState state =
        BuildViewNavigationWorkflowState(project, inspector, selection);

    EXPECT_EQ(state.simple.selection.behaviorId, selection.behaviorId);
    EXPECT_EQ(state.pro.selection.nodeId, selection.nodeId);
    EXPECT_EQ(state.csharpSource.selection.sourceLink.sourceFile,
              selection.sourceLink.sourceFile);
    EXPECT_FALSE(state.simple.canMutateSource);
    EXPECT_FALSE(state.simple.canApplyPatch);
    EXPECT_FALSE(state.csharpSource.canMutateSource);
    EXPECT_TRUE(state.simple.disclosesOpaqueDeferredUnsupported);
    EXPECT_FALSE(state.diagnostics.empty());

    fs::remove_all(root);
}
