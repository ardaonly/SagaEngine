// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

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
    std::ofstream output(path, std::ios::binary);
    output << bytes;
}

fs::path FixtureRoot()
{
    const auto root = fs::temp_directory_path() /
        ("saga-editor-workspace-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    Write(root / "Project.sagaproj", "{}\n");
    Write(root / "Scenes/Main.sagascene",
        R"({"schemaVersion":1,"sceneId":"main","entities":[{"entityId":"root","name":"Root","components":[{"type":"Transform","position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]}]}]})");
    Write(root / "Input/Input.saga.json",
        R"({"schemaVersion":1,"inputMapId":"main","actions":[]})");
    Write(root / "Scripts/Door.cs", "public class Door {}\n");
    Write(root / "Assets/Ground.asset.json", "{}\n");
    Write(root / "SagaPackage.lock", "{}\n");
    return root;
}

SagaEditor::ProjectToolResult ProjectResult(const fs::path& root)
{
    SagaEditor::ProjectToolResult result;
    result.passed = true;
    result.project.projectId = "8f21adcb-0085-4ef0-b54e-c1ed2501cf44";
    result.project.displayName = "Fixture";
    result.project.projectRoot = root;
    result.project.manifestPath = root / "Project.sagaproj";
    result.project.sources = {
        {"scene", "main", "scene", "Scenes/Main.sagascene", root / "Scenes/Main.sagascene"},
        {"inputMap", "main-input", "input-map", "Input/Input.saga.json", root / "Input/Input.saga.json"},
        {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"},
        {"asset", "ground", "primitive", "Assets/Ground.asset.json", root / "Assets/Ground.asset.json"},
    };
    return result;
}
}

TEST(EditorDocumentWorkspaceTests, OpensCanonicalDocumentsAndClassifiesTrackedLock)
{
    const auto root = FixtureRoot();
    auto client = std::make_shared<FakeProjectClient>();
    client->result = ProjectResult(root);
    SagaEditor::EditorDocumentWorkspace workspace(client);

    ASSERT_TRUE(workspace.OpenProject(root));
    ASSERT_NE(workspace.ProjectSource(), nullptr);
    ASSERT_NE(workspace.ActiveScene(), nullptr);
    EXPECT_EQ(workspace.Scenes().size(), 1U);
    EXPECT_EQ(workspace.Scripts().size(), 1U);
    EXPECT_EQ(workspace.InputMaps().size(), 1U);
    const auto lock = std::find_if(workspace.SourceListing().begin(), workspace.SourceListing().end(),
        [](const auto& entry) { return entry.kind == "package-lock"; });
    ASSERT_NE(lock, workspace.SourceListing().end());
    EXPECT_EQ(lock->classification,
              SagaEditor::ProjectSourceClassification::GeneratedStateTrackedException);
    fs::remove_all(root);
}

TEST(EditorDocumentWorkspaceTests, FailedOpenPreservesCurrentWorkspace)
{
    const auto root = FixtureRoot();
    auto client = std::make_shared<FakeProjectClient>();
    client->result = ProjectResult(root);
    SagaEditor::EditorDocumentWorkspace workspace(client);
    ASSERT_TRUE(workspace.OpenProject(root));
    client->result = {};
    EXPECT_FALSE(workspace.OpenProject(root / "missing"));
    EXPECT_TRUE(workspace.IsOpen());
    EXPECT_EQ(workspace.Project()->name, "Fixture");
    fs::remove_all(root);
}
