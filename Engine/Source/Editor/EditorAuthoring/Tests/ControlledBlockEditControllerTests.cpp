// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Diagnostics/EditorDiagnosticsService.h"
#include "SagaEditor/Project/ProjectManager.h"
#include "SagaEditor/Scripting/SagaScriptLanguageServiceSession.h"
#include "SagaEditor/VisualBlocks/ControlledBlockEditController.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

namespace
{
namespace fs = std::filesystem;

class PassedProjectClient final : public SagaEditor::IProjectToolClient
{
public:
    SagaEditor::ProjectToolResult result;
    SagaEditor::ProjectToolResult Validate(const fs::path&) const override { return result; }
    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override { return result; }
};

void Write(const fs::path& path, std::string_view value)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << value;
}
}

TEST(ControlledBlockEditControllerTests, PlansAppliesRejectsStaleAndSharesUndo)
{
    const auto root = fs::temp_directory_path() /
        ("saga-controlled-edit-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto projectPath = root / "Project.sagaproj";
    const auto scriptPath = root / "Scripts/ArenaRushGameMode.cs";
    const std::string source =
        "namespace ArenaRush; public sealed class ArenaRushGameMode { "
        "private const int EnergyCoreScore = 10; "
        "private const double EnergyCoreRespawnSeconds = 2.0; }\n";
    Write(projectPath, "{}\n");
    Write(root / "SagaPackage.lock", "{}\n");
    Write(scriptPath, source);
    auto client = std::make_shared<PassedProjectClient>();
    client->result.passed = true;
    client->result.project.projectId = "controlled-edit-project";
    client->result.project.displayName = "Controlled Edit";
    client->result.project.projectRoot = root;
    client->result.project.manifestPath = projectPath;
    client->result.project.sources = {
        {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"}};
    SagaEditor::EditorDocumentWorkspace workspace(client);
    ASSERT_TRUE(workspace.OpenProject(projectPath));
    ASSERT_EQ(workspace.Scripts().size(), 1u);
    SagaEditor::EditorDiagnosticsService diagnostics;
    SagaEditor::SagaScriptLanguageServiceSession language(workspace, diagnostics);
    language.Refresh();
    ASSERT_EQ(language.State(), SagaEditor::LanguageServiceState::Ready)
        << language.LastDiagnostic();
    SagaEditor::UndoRedoStack journal;
    SagaEditor::ControlledBlockEditController controller(language, journal);
    auto& document = *workspace.Scripts().front();
    const auto initialProjection = controller.AnalyzeProjection(document);
    ASSERT_TRUE(initialProjection.Succeeded()) << initialProjection.diagnostic;
    EXPECT_EQ(initialProjection.projection->EditableFields().size(), 2u);
    const auto invalid = controller.Plan(document,
        "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer", "0");
    EXPECT_FALSE(invalid.accepted);
    EXPECT_EQ(controller.State(),
        SagaEditor::ControlledBlockEditState::SemanticRejected);
    const auto opaque = controller.Plan(document, "field://unsupported", "20");
    EXPECT_FALSE(opaque.accepted);
    EXPECT_EQ(controller.State(),
        SagaEditor::ControlledBlockEditState::OpaqueReadOnly);
    const auto score = controller.Plan(document,
        "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer", "20");
    ASSERT_TRUE(score.accepted)
        << (score.diagnostics.empty() ? std::string{} : score.diagnostics.front());
    ASSERT_TRUE(controller.Apply(document, score)) << controller.Diagnostic();
    EXPECT_EQ(document.Text().find("EnergyCoreScore = 20"), source.find("EnergyCoreScore = 10"));
    EXPECT_NE(document.Text().find("EnergyCoreRespawnSeconds = 2.0"), std::string::npos);
    EXPECT_TRUE(document.Snapshot().dirty);
    language.Refresh();
    const auto editedProjection = controller.AnalyzeProjection(document);
    ASSERT_TRUE(editedProjection.Succeeded()) << editedProjection.diagnostic;
    ASSERT_EQ(editedProjection.projection->EditableFields().size(), 2u);
    EXPECT_EQ(editedProjection.projection->EditableFields()[0].value, "20");
    journal.Undo();
    EXPECT_EQ(document.Text(), source);
    journal.Redo();
    EXPECT_NE(document.Text().find("EnergyCoreScore = 20"), std::string::npos);

    const auto respawn = controller.Plan(document,
        "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer", "2");
    ASSERT_TRUE(respawn.accepted);
    EXPECT_EQ(respawn.replacement, "2.0");
    ASSERT_TRUE(document.ReplaceText(document.Text() + "// custom\n"));
    EXPECT_FALSE(controller.Apply(document, respawn));
    EXPECT_EQ(controller.State(), SagaEditor::ControlledBlockEditState::StaleDocument);

    const auto external = controller.Plan(document,
        "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer", "30");
    ASSERT_TRUE(external.accepted);
    Write(scriptPath, source + "// external\n");
    EXPECT_FALSE(controller.Apply(document, external));
    EXPECT_EQ(controller.State(), SagaEditor::ControlledBlockEditState::Conflict);
    language.Shutdown();
    fs::remove_all(root);
}
