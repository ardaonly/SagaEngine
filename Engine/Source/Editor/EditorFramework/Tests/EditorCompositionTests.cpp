/// @file EditorCompositionTests.cpp
/// @brief Unit tests for editor composition artifact consumption.

#include "SagaEditor/Composition/EditorCompositionArtifactLoader.h"
#include "SagaEditor/Composition/EditorCompositionDiff.h"
#include "SagaEditor/Composition/EditorCompositionResolver.h"
#include "SagaEditor/Customization/EditorCustomizationOverlayLoader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using namespace SagaEditor;
namespace fs = std::filesystem;

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

[[nodiscard]] std::string ValidArtifactJson()
{
    return R"({
      "artifactKind": "saga.editor.composition",
      "artifactVersion": 1,
      "schemaPackage": "saga.editor",
      "schemaPackageVersion": 1,
      "compositionId": "saga.editor.default",
      "sourceMapRef": "Build/Graphs/SourceMaps/saga.editor.default.sourcemap.json",
      "metadata": {
        "displayName": "Saga Default Editor",
        "description": "Compiled test fixture for editor composition."
      },
      "panels": [
        {
          "id": "saga.panel.hierarchy",
          "displayName": "Hierarchy",
          "kind": "builtin",
          "defaultVisible": true
        },
        {
          "id": "saga.panel.internal.serviceGraph",
          "displayName": "Service Graph",
          "kind": "builtin",
          "defaultVisible": false,
          "internalOnly": true
        }
      ],
      "actions": [
        {
          "id": "saga.action.workspace.reset",
          "displayName": "Reset Workspace",
          "category": "Workspace",
          "safeInProduct": true
        },
        {
          "id": "saga.action.internal.mutateServiceGraph",
          "displayName": "Mutate Service Graph",
          "category": "Internal",
          "safeInProduct": false,
          "internalOnly": true
        }
      ],
      "menus": [
        {
          "id": "saga.menu.workspace",
          "displayName": "Workspace",
          "items": [
            { "kind": "action", "actionId": "saga.action.workspace.reset" }
          ]
        }
      ],
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

[[nodiscard]] EditorCompositionManifest MakeManifest()
{
    EditorCompositionManifest manifest;
    manifest.manifestKind = "saga.editor.composition.manifest";
    manifest.manifestVersion = 1;
    manifest.compositionId = "saga.editor.default";
    manifest.artifactPath = "saga.editor.default.composition.json";
    manifest.artifactHash = "test-artifact-hash";
    manifest.schemaPackage = "saga.editor";
    manifest.schemaPackageVersion = 1;
    manifest.compilerVersion = "test";
    return manifest;
}

[[nodiscard]] EditorCompositionArtifact LoadValidArtifact()
{
    EditorCompositionArtifactLoader loader;
    EditorCompositionArtifactLoadResult result = loader.LoadArtifactText(ValidArtifactJson(), "fixture");
    EXPECT_TRUE(result.artifact.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(result.diagnostics));
    return *result.artifact;
}

TEST(EditorCompositionArtifactLoaderTest, LoadsCompiledSdeArtifactShape)
{
    EditorCompositionArtifactLoader loader;

    EditorCompositionArtifactLoadResult result = loader.LoadArtifactText(ValidArtifactJson(), "fixture");

    ASSERT_TRUE(result.artifact.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(result.diagnostics));
    EXPECT_EQ(result.artifact->artifactKind, "saga.editor.composition");
    EXPECT_EQ(result.artifact->schemaPackage, "saga.editor");
    ASSERT_EQ(result.artifact->panels.size(), 2u);
    EXPECT_EQ(result.artifact->panels[0].id, "saga.panel.hierarchy");
}

TEST(EditorCompositionArtifactLoaderTest, RejectsUnsupportedFutureArtifactVersion)
{
    EditorCompositionArtifactLoader loader;
    const std::string futureArtifact = R"({
      "artifactKind": "saga.editor.composition",
      "artifactVersion": 999,
      "schemaPackage": "saga.editor",
      "schemaPackageVersion": 1,
      "compositionId": "saga.editor.default"
    })";

    EditorCompositionArtifactLoadResult result = loader.LoadArtifactText(futureArtifact, "future");

    EXPECT_TRUE(result.artifact.has_value());
    EXPECT_TRUE(HasBlockingCompositionDiagnostic(result.diagnostics));
}

TEST(EditorCompositionArtifactLoaderTest, LoadsArtifactThroughEmittedManifest)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_editor_composition_manifest_load";
    fs::remove_all(root);
    const std::string artifactText = ValidArtifactJson();
    WriteFile(root / "saga.editor.composition.json", artifactText);
    WriteFile(root / "saga.editor.composition.manifest.json",
              std::string{R"({
        "manifestKind": "saga.editor.composition.manifest",
        "manifestVersion": 1,
        "compositionId": "saga.editor.default",
        "artifactPath": "saga.editor.composition.json",
        "diagnosticsPath": "saga.editor.composition.diagnostics.json",
        "sourceMapPath": "saga.editor.composition.sourcemap.json",
        "dependencyManifestPath": "saga.editor.composition.dependencies.json",
        "artifactHash": ")" + StableHashBytes(artifactText) + R"(",
        "schemaPackage": "saga.editor",
        "schemaPackageVersion": 1,
        "compilerVersion": "test"
      })"});

    EditorCompositionArtifactLoader loader;
    EditorCompositionLoadResult result =
        loader.LoadFromManifestFile(root / "saga.editor.composition.manifest.json");

    ASSERT_TRUE(result.manifest.has_value());
    ASSERT_TRUE(result.artifact.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(result.diagnostics));
    EXPECT_EQ(result.artifact->compositionId, "saga.editor.default");
}

TEST(EditorCompositionArtifactLoaderTest, RejectsArtifactHashMismatch)
{
    const fs::path root =
        fs::temp_directory_path() / "saga_editor_composition_manifest_hash_mismatch";
    fs::remove_all(root);
    WriteFile(root / "saga.editor.composition.json", ValidArtifactJson());
    WriteFile(root / "saga.editor.composition.manifest.json",
              R"({
        "manifestKind": "saga.editor.composition.manifest",
        "manifestVersion": 1,
        "compositionId": "saga.editor.default",
        "artifactPath": "saga.editor.composition.json",
        "artifactHash": "not-the-real-hash",
        "schemaPackage": "saga.editor",
        "schemaPackageVersion": 1,
        "compilerVersion": "test"
      })");

    EditorCompositionArtifactLoader loader;
    EditorCompositionLoadResult result =
        loader.LoadFromManifestFile(root / "saga.editor.composition.manifest.json");

    EXPECT_TRUE(result.artifact.has_value());
    EXPECT_TRUE(HasBlockingCompositionDiagnostic(result.diagnostics));
}

TEST(EditorCustomizationOverlayLoaderTest, LoadsSafeOverlayShape)
{
    EditorCustomizationOverlayLoader loader;
    const std::string overlayJson = R"({
      "schemaVersion": 1,
      "baseCompositionId": "saga.editor.default",
      "baseArtifactHash": "test-artifact-hash",
      "overlayId": "user.workspace.default",
      "layoutOverrides": [
        {
          "workspaceId": "saga.workspace.default",
          "panelId": "saga.panel.hierarchy",
          "placement": "left",
          "visible": false
        }
      ],
      "shortcutOverrides": [
        {
          "actionId": "saga.action.workspace.reset",
          "chord": "Ctrl+Alt+R"
        }
      ],
      "visibilityOverrides": [],
      "toolbarOverrides": []
    })";

    EditorCustomizationOverlayLoadResult result = loader.LoadText(overlayJson, "overlay");

    ASSERT_TRUE(result.overlay.has_value());
    EXPECT_FALSE(HasErrorCompositionDiagnostic(result.diagnostics));
    EXPECT_EQ(result.overlay->overlayId, "user.workspace.default");
    ASSERT_EQ(result.overlay->layoutOverrides.size(), 1u);
    EXPECT_FALSE(result.overlay->layoutOverrides[0].visible);
}

TEST(EditorCompositionResolverTest, AppliesSafeOverlayToCompiledArtifact)
{
    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = 1;
    overlay.baseCompositionId = "saga.editor.default";
    overlay.baseArtifactHash = "test-artifact-hash";
    overlay.overlayId = "user.workspace.default";
    overlay.layoutOverrides.push_back({ "saga.workspace.default",
                                        "saga.panel.hierarchy",
                                        "left",
                                        false });
    overlay.shortcutOverrides.push_back({ "saga.action.workspace.reset", "Ctrl+Alt+R" });

    EditorCompositionResolver resolver;
    const EditorCompositionArtifact artifact = LoadValidArtifact();
    const std::vector<EditorCustomizationOverlay> overlays{ overlay };

    ResolvedEditorCompositionSnapshot snapshot =
        resolver.Resolve(MakeManifest(), artifact, overlays);

    ASSERT_TRUE(snapshot.isUsable);
    ASSERT_EQ(snapshot.panels.size(), 2u);
    EXPECT_EQ(snapshot.panels[0].definition.id, "saga.panel.hierarchy");
    EXPECT_FALSE(snapshot.panels[0].visible);
    EXPECT_EQ(snapshot.panels[0].placement, "left");
    ASSERT_EQ(snapshot.shortcuts.size(), 1u);
    EXPECT_EQ(snapshot.shortcuts[0].chord, "Ctrl+Alt+R");
}

TEST(EditorCompositionResolverTest, RejectsUnsafeInternalMutationInOverlay)
{
    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = 1;
    overlay.baseCompositionId = "saga.editor.default";
    overlay.overlayId = "user.workspace.default";
    overlay.visibilityOverrides.push_back({ "saga.panel.internal.serviceGraph", true });

    EditorCompositionResolver resolver;
    const EditorCompositionArtifact artifact = LoadValidArtifact();
    const std::vector<EditorCustomizationOverlay> overlays{ overlay };

    ResolvedEditorCompositionSnapshot snapshot =
        resolver.Resolve(MakeManifest(), artifact, overlays);

    EXPECT_FALSE(snapshot.isUsable);
    EXPECT_TRUE(HasBlockingCompositionDiagnostic(snapshot.diagnostics));
}

TEST(EditorCompositionResolverTest, DetectsShortcutCollisionsAfterOverlay)
{
    EditorCompositionArtifact artifact = LoadValidArtifact();
    artifact.actions.push_back({ "saga.action.workspace.save",
                                 "Save Workspace",
                                 "Workspace",
                                 true,
                                 false });
    artifact.shortcuts.push_back({ "saga.shortcut.workspace.save",
                                   "saga.action.workspace.save",
                                   "Ctrl+S" });

    EditorCustomizationOverlay overlay;
    overlay.schemaVersion = 1;
    overlay.baseCompositionId = "saga.editor.default";
    overlay.overlayId = "user.workspace.default";
    overlay.shortcutOverrides.push_back({ "saga.action.workspace.reset", "Ctrl+S" });

    EditorCompositionResolver resolver;
    const std::vector<EditorCustomizationOverlay> overlays{ overlay };

    ResolvedEditorCompositionSnapshot snapshot =
        resolver.Resolve(MakeManifest(), artifact, overlays);

    EXPECT_FALSE(snapshot.isUsable);
    EXPECT_TRUE(HasErrorCompositionDiagnostic(snapshot.diagnostics));
}

TEST(EditorCompositionDiffTest, ReportsTopLevelArtifactChanges)
{
    EditorCompositionArtifact oldArtifact = LoadValidArtifact();
    EditorCompositionArtifact newArtifact = oldArtifact;
    newArtifact.panels.push_back({ "saga.panel.assetBrowser",
                                   "Asset Browser",
                                   "builtin",
                                   true,
                                   false,
                                   {} });

    EditorCompositionDiff diff = DiffEditorCompositionArtifacts(oldArtifact, newArtifact);

    ASSERT_EQ(diff.entries.size(), 1u);
    EXPECT_EQ(diff.entries[0].kind, EditorCompositionDiffKind::AddedDefinition);
    EXPECT_EQ(diff.entries[0].definitionType, "panel");
    EXPECT_EQ(diff.entries[0].id, "saga.panel.assetBrowser");
}

} // namespace
