/// @file ComposerTests.cpp
/// @brief Unit tests for SagaEditorComposer core models.

#include "SagaEditor/Composer/ComposerArtifactSummary.h"
#include "SagaEditor/Composer/ComposerBuildPlan.h"
#include "SagaEditor/Composer/ComposerDiagnosticsModel.h"
#include "SagaEditor/Composer/ComposerEditCommand.h"
#include "SagaEditor/Composer/ComposerSourceIndex.h"
#include "SagaEditor/Composer/ComposerWorkspace.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace
{

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeRoot(const std::string& name)
{
    const fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

[[nodiscard]] std::string ReadFile(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string SourceText()
{
    return R"(sde version 1

package saga.editor.product
import saga.editor

instance EditorComposition saga.editor.default {
  displayName = "SagaEditor Default Composition"
}

instance EditorPanel saga.panel.viewport {
  composition = saga.editor.default
  displayName = "World Viewport"
  kind = "builtin"
  defaultVisible = true
  internalOnly = false
}

instance EditorAction saga.command.file.save {
  composition = saga.editor.default
  displayName = "Save Scene"
  category = "File"
}

instance EditorWorkspace saga.workspace.default {
  composition = saga.editor.default
  displayName = "Default Workspace"
  layoutId = saga.layout.default
}
)";
}

} // namespace

TEST(SagaEditorComposerCoreTest, ResolvesWorkspacePaths)
{
    const fs::path root = MakeRoot("saga_editor_composer_paths");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);

    EXPECT_EQ(paths.sourceRoot, root / "Editor" / "CompositionSources");
    EXPECT_EQ(paths.artifactPath,
              root / "Build" / "Artifacts" / "EditorComposition" /
                  "saga.editor.composition.json");
    EXPECT_EQ(paths.checkpointRoot,
              root / ".saga" / "composer" / "checkpoints");
}

TEST(SagaEditorComposerCoreTest, MissingSourceWorkspaceReportsDiagnostic)
{
    const fs::path root = MakeRoot("saga_editor_composer_missing_source");
    const SagaEditor::ComposerSourceIndex index =
        SagaEditor::BuildComposerSourceIndex(
            SagaEditor::ResolveComposerWorkspacePaths(root));

    ASSERT_FALSE(index.diagnostics.empty());
    EXPECT_EQ(index.diagnostics.front().code,
              "ComposerSource.MissingSourceRoot");
}

TEST(SagaEditorComposerCoreTest, SourceIndexListsCompositionItems)
{
    const fs::path root = MakeRoot("saga_editor_composer_source_index");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);
    WriteFile(paths.sourceRoot / "source" / "composition.sde", SourceText());

    const SagaEditor::ComposerSourceIndex index =
        SagaEditor::BuildComposerSourceIndex(paths);

    EXPECT_NE(SagaEditor::FindComposerSourceItem(index, "saga.editor.default"),
              nullptr);
    const auto* panel =
        SagaEditor::FindComposerSourceItem(index, "saga.panel.viewport");
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->kind, SagaEditor::ComposerSourceKind::Panel);
    const auto* displayName =
        SagaEditor::FindComposerSourceField(*panel, "displayName");
    ASSERT_NE(displayName, nullptr);
    EXPECT_EQ(displayName->value, "World Viewport");
}

TEST(SagaEditorComposerCoreTest, EditSessionCreatesPendingDiffAndSavesSource)
{
    const fs::path root = MakeRoot("saga_editor_composer_edit_save");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);
    const fs::path sourceFile = paths.sourceRoot / "source" / "composition.sde";
    const fs::path artifactFile = paths.artifactPath;
    WriteFile(sourceFile, SourceText());
    WriteFile(artifactFile, "{\"panels\":[]}");

    SagaEditor::ComposerEditSession edits;
    edits.Reset(SagaEditor::BuildComposerSourceIndex(paths));
    const SagaEditor::ComposerEditResult pending =
        edits.SetField("saga.panel.viewport",
                       "displayName",
                       "Primary Viewport");

    ASSERT_TRUE(pending.ok);
    ASSERT_EQ(edits.PendingEdits().size(), 1u);
    EXPECT_EQ(edits.PendingEdits().front().oldValue, "World Viewport");
    EXPECT_EQ(edits.PendingEdits().front().newValue, "Primary Viewport");

    const SagaEditor::ComposerEditResult saved = edits.Save(paths);
    EXPECT_TRUE(saved.ok);
    EXPECT_NE(ReadFile(sourceFile).find("Primary Viewport"), std::string::npos);
    EXPECT_EQ(ReadFile(artifactFile), "{\"panels\":[]}");
    EXPECT_TRUE(fs::exists(paths.checkpointRoot));
    EXPECT_FALSE(fs::is_empty(paths.checkpointRoot));
}

TEST(SagaEditorComposerCoreTest, InvalidEditsFailWithoutMutation)
{
    const fs::path root = MakeRoot("saga_editor_composer_invalid_edit");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);
    WriteFile(paths.sourceRoot / "source" / "composition.sde", SourceText());

    SagaEditor::ComposerEditSession edits;
    edits.Reset(SagaEditor::BuildComposerSourceIndex(paths));

    EXPECT_FALSE(edits.SetField("saga.panel.missing",
                                "displayName",
                                "Missing").ok);
    EXPECT_FALSE(edits.SetField("saga.panel.viewport",
                                "composition",
                                "bad").ok);
    EXPECT_FALSE(edits.SetField("saga.panel.viewport",
                                "defaultVisible",
                                "sometimes").ok);
    EXPECT_TRUE(edits.PendingEdits().empty());
}

TEST(SagaEditorComposerCoreTest, ArtifactAndDiagnosticsReadersLoadReports)
{
    const fs::path root = MakeRoot("saga_editor_composer_reports");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);
    WriteFile(paths.manifestPath,
              R"({"compositionId":"saga.editor.default","artifactHash":"abc"})");
    WriteFile(paths.artifactPath,
              R"({"panels":[{}],"actions":[{},{}],"menus":[{}],"toolbars":[],"shortcuts":[],"workspaces":[{}],"editorModes":[{}]})");
    WriteFile(paths.diagnosticsPath,
              R"([{"severity":"Error","code":"E.Test","message":"Failure","sourceFile":"source/composition.sde","line":12,"column":4}])");

    const SagaEditor::ComposerArtifactSummary summary =
        SagaEditor::LoadComposerArtifactSummary(paths);
    EXPECT_TRUE(summary.manifestFound);
    EXPECT_EQ(summary.compositionId, "saga.editor.default");
    EXPECT_EQ(summary.actionCount, 2);
    EXPECT_EQ(summary.modeCount, 1);

    const SagaEditor::ComposerDiagnosticsModel diagnostics =
        SagaEditor::LoadComposerDiagnostics(paths);
    ASSERT_EQ(diagnostics.rows.size(), 1u);
    EXPECT_EQ(diagnostics.rows.front().code, "E.Test");
    EXPECT_EQ(diagnostics.rows.front().line, 12);
}

TEST(SagaEditorComposerCoreTest, BuildPlanIsDeterministic)
{
    const fs::path root = MakeRoot("saga_editor_composer_build_plan");
    const SagaEditor::ComposerWorkspacePaths paths =
        SagaEditor::ResolveComposerWorkspacePaths(root);

    const SagaEditor::ComposerBuildPlan plan =
        SagaEditor::MakeComposerBuildPlan(paths, root / "bin" / "SagaEditorComposer");

    EXPECT_EQ(plan.arguments[0], "editor-composition");
    EXPECT_EQ(plan.arguments[1], "build");
    EXPECT_EQ(plan.arguments[2], "--project");
    EXPECT_EQ(plan.arguments[3], root.string());
    EXPECT_EQ(plan.arguments[4], "--tool");
    EXPECT_NE(plan.displayCommand.find("saga-pipeline"), std::string::npos);
    EXPECT_NE(plan.displayCommand.find("saga-editor-composition-compiler"),
              std::string::npos);
}
