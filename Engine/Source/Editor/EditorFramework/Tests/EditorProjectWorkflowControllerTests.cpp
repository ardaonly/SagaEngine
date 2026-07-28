// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/EditorProjectWorkflowController.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

namespace
{

namespace fs = std::filesystem;
using namespace SagaEditor;

void Write(const fs::path& path, std::string_view bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << bytes;
}

void WriteProject(const fs::path& root)
{
    Write(root / "Project.sagaproj", "{}\n");
    Write(root / "Scenes/Main.sagascene",
        R"({"schemaVersion":1,"sceneId":"main","entities":[{"entityId":"root","name":"Root","components":[{"type":"Transform","position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}]}]})");
    Write(root / "Scripts/Door.cs", "public class Door {}\n");
}

ProjectToolResult ProjectResult(const fs::path& root, std::string name)
{
    ProjectToolResult result;
    result.passed = true;
    result.project.projectId = "workflow-" + name;
    result.project.displayName = std::move(name);
    result.project.projectRoot = root;
    result.project.manifestPath = root / "Project.sagaproj";
    result.project.sources = {
        {"scene", "main", "scene", "Scenes/Main.sagascene",
         root / "Scenes/Main.sagascene"},
        {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"},
    };
    return result;
}

class WorkflowProjectClient final : public IProjectToolClient
{
public:
    mutable bool failValidation = false;
    mutable bool failCreation = false;

    ProjectToolResult Validate(const fs::path& manifest) const override
    {
        if (failValidation) return {};
        const auto root = manifest.parent_path();
        return ProjectResult(root, root.filename().string());
    }

    ProjectToolResult CreateStarterArena(
        const ProjectTemplateRequest& request) const override
    {
        if (failCreation) return {};
        WriteProject(request.destination);
        return ProjectResult(request.destination, request.displayName);
    }
};

class WorkflowFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
            ("saga-project-workflow-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        first = root / "First";
        second = root / "Second";
        WriteProject(first);
        WriteProject(second);
        client = std::make_shared<WorkflowProjectClient>();
        workspace = std::make_unique<EditorDocumentWorkspace>(client);
        ASSERT_TRUE(workspace->OpenProject(first));
    }

    void TearDown() override
    {
        workspace.reset();
        std::error_code error;
        fs::remove_all(root, error);
    }

    void MakeDirty()
    {
        ASSERT_EQ(workspace->Scripts().size(), 1U);
        ASSERT_TRUE(workspace->Scripts().front()->ReplaceText(
            "public class Door { public bool Open; }\n"));
        ASSERT_FALSE(workspace->DirtyDocumentIds().empty());
    }

    fs::path root;
    fs::path first;
    fs::path second;
    std::shared_ptr<WorkflowProjectClient> client;
    std::unique_ptr<EditorDocumentWorkspace> workspace;
};

TEST_F(WorkflowFixture, CanCloseCompletesForCleanWorkspace)
{
    EditorProjectWorkflowController controller(*workspace, {});
    EXPECT_EQ(controller.CanClose(), ProjectWorkflowOutcome::Completed);
}

TEST_F(WorkflowFixture, CanCloseHonorsSaveDiscardAndCancel)
{
    MakeDirty();
    ProjectWorkflowPrompts prompts;
    prompts.confirmDirtyDocuments = [](const auto&)
    {
        return DirtyDocumentDecision::Cancel;
    };
    EditorProjectWorkflowController cancelled(*workspace, prompts);
    EXPECT_EQ(cancelled.CanClose(), ProjectWorkflowOutcome::Cancelled);

    prompts.confirmDirtyDocuments = [](const auto&)
    {
        return DirtyDocumentDecision::Discard;
    };
    EditorProjectWorkflowController discarded(*workspace, prompts);
    EXPECT_EQ(discarded.CanClose(), ProjectWorkflowOutcome::Completed);
    EXPECT_FALSE(workspace->DirtyDocumentIds().empty());

    prompts.confirmDirtyDocuments = [](const auto&)
    {
        return DirtyDocumentDecision::SaveAll;
    };
    EditorProjectWorkflowController saved(*workspace, prompts);
    EXPECT_EQ(saved.CanClose(), ProjectWorkflowOutcome::Completed);
    EXPECT_TRUE(workspace->DirtyDocumentIds().empty());
}

TEST_F(WorkflowFixture, DialogCancellationDoesNotReplaceWorkspace)
{
    ProjectWorkflowPrompts prompts;
    prompts.chooseExistingProject = [] {
        return std::optional<fs::path>{};
    };
    prompts.requestNewProject = [] {
        return std::optional<ProjectCreationPrompt>{};
    };
    EditorProjectWorkflowController controller(*workspace, prompts);

    EXPECT_EQ(controller.OpenProjectPrompted(),
              ProjectWorkflowOutcome::Cancelled);
    EXPECT_EQ(controller.CreateStarterArenaPrompted(),
              ProjectWorkflowOutcome::Cancelled);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "First");
}

TEST_F(WorkflowFixture, OpenIsTransactionalAcrossSuccessAndFailure)
{
    ProjectWorkflowPrompts prompts;
    prompts.chooseExistingProject = [this] {
        return std::optional<fs::path>{second};
    };
    EditorProjectWorkflowController controller(*workspace, prompts);
    EXPECT_EQ(controller.OpenProjectPrompted(),
              ProjectWorkflowOutcome::Completed);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "Second");

    client->failValidation = true;
    prompts.chooseExistingProject = [this] {
        return std::optional<fs::path>{first};
    };
    EditorProjectWorkflowController failing(*workspace, prompts);
    EXPECT_EQ(failing.OpenProjectPrompted(), ProjectWorkflowOutcome::Failed);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "Second");
}

TEST_F(WorkflowFixture, CreateIsTransactionalAcrossSuccessAndFailure)
{
    const auto created = root / "Created";
    ProjectWorkflowPrompts prompts;
    prompts.requestNewProject = [created] {
        return std::optional<ProjectCreationPrompt>{
            ProjectCreationPrompt{"Created Project", created}};
    };
    EditorProjectWorkflowController controller(*workspace, prompts);
    EXPECT_EQ(controller.CreateStarterArenaPrompted(),
              ProjectWorkflowOutcome::Completed);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "Created Project");

    client->failCreation = true;
    const auto failed = root / "Failed";
    prompts.requestNewProject = [failed] {
        return std::optional<ProjectCreationPrompt>{
            ProjectCreationPrompt{"Failed Project", failed}};
    };
    EditorProjectWorkflowController failing(*workspace, prompts);
    EXPECT_EQ(failing.CreateStarterArenaPrompted(),
              ProjectWorkflowOutcome::Failed);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "Created Project");
}

TEST_F(WorkflowFixture, DirtyCancellationVetoesPromptedOpen)
{
    MakeDirty();
    ProjectWorkflowPrompts prompts;
    prompts.chooseExistingProject = [this] {
        return std::optional<fs::path>{second};
    };
    prompts.confirmDirtyDocuments = [](const auto&)
    {
        return DirtyDocumentDecision::Cancel;
    };
    EditorProjectWorkflowController controller(*workspace, prompts);
    EXPECT_EQ(controller.OpenProjectPrompted(),
              ProjectWorkflowOutcome::Cancelled);
    ASSERT_NE(workspace->Project(), nullptr);
    EXPECT_EQ(workspace->Project()->name, "First");
}

} // namespace
