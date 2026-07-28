// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
namespace fs = std::filesystem;

class FakeProjectClient final : public SagaEditor::IProjectToolClient
{
public:
    SagaEditor::ProjectToolResult result;
    SagaEditor::ProjectToolResult Validate(const fs::path&) const override { return result; }
    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override { return result; }
};

void Write(const fs::path& path, std::string_view bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}
}

TEST(ScriptAnalysisCoordinatorTests, RejectsDirtyBuffersWithoutStartingTools)
{
    const auto root = fs::temp_directory_path() / ("saga-script-analysis-dirty-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Write(root / "Project.sagaproj", "{}\n");
    Write(root / "Scripts/Door.cs", "public class Door {}\n");
    auto client = std::make_shared<FakeProjectClient>();
    client->result.passed = true;
    client->result.project.projectId = "5734c541-95e2-49a2-a284-d82ef2fc3db4";
    client->result.project.displayName = "Analysis Fixture";
    client->result.project.projectRoot = root;
    client->result.project.manifestPath = root / "Project.sagaproj";
    client->result.project.sources = {
        {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"}};
    SagaEditor::EditorDocumentWorkspace workspace(client);
    ASSERT_TRUE(workspace.OpenProject(root));
    ASSERT_FALSE(workspace.Scripts().empty());
    ASSERT_TRUE(workspace.Scripts().front()->ReplaceText("public class Door { }\n"));

    SagaEditor::ScriptAnalysisToolchain missingTools;
    missingTools.engineRoot = root;
    missingTools.managedRuntime.testStage = root / "must-not-run-stage";
    const auto result = SagaEditor::ScriptAnalysisCoordinator(missingTools).Analyze(workspace);
    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "Script.Analysis.UnsavedDocuments");
    EXPECT_EQ(result.packageProcess.classification,
        SagaEditor::ToolProcessExitClassification::NotStarted);
    fs::remove_all(root);
}
