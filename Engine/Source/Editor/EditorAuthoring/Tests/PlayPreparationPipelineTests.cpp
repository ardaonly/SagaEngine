// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/PlayPreparationPipeline.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>
#include <algorithm>
#include <map>

namespace
{
std::filesystem::path StarterArenaProject()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Samples/StarterArena/StarterArena.sagaproj";
}

std::filesystem::path RuntimeExecutable()
{
    return std::filesystem::path(SAGA_BUILD_ROOT) / "bin/SagaRuntime";
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

class SnapshotProjectClient final : public SagaEditor::IProjectToolClient
{
public:
    SagaEditor::ProjectToolResult result;
    SagaEditor::ProjectToolResult Validate(const std::filesystem::path&) const override
    { return result; }
    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override { return result; }
};

std::pair<std::filesystem::path, std::shared_ptr<SnapshotProjectClient>> SnapshotFixture()
{
    const auto root = std::filesystem::temp_directory_path() / ("saga-play-snapshot-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "Scripts");
    std::ofstream(root / "Project.sagaproj") << "{}\n";
    std::ofstream(root / "SagaPackage.lock") << "{\"schemaVersion\":1}\n";
    std::ofstream(root / "Scripts/Game.cs") << "public class Game {}\n";
    auto client = std::make_shared<SnapshotProjectClient>();
    client->result.passed = true;
    client->result.project.projectId = "d486de91-a99f-4430-a791-ef2a4fdc8d2b";
    client->result.project.projectRoot = root;
    client->result.project.manifestPath = root / "Project.sagaproj";
    client->result.project.sources = {
        {"scriptFolder", "scripts", "csharp", "Scripts", root / "Scripts"}};
    return {root, client};
}
}

TEST(PlayUnsavedDocumentPolicyTests, DirtySourceBlocksPreparationWithoutSaving)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    ASSERT_EQ(workspace.Scripts().size(), 1U);
    const auto original = workspace.Scripts().front()->Text();
    ASSERT_TRUE(workspace.Scripts().front()->ReplaceText(original + "\n"));

    const auto result = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace,
        {"dirty-policy", RuntimeExecutable(), {}, {}, {}});

    EXPECT_FALSE(result.passed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, "Play.Prepare.UnsavedDocuments");
    EXPECT_EQ(workspace.Scripts().front()->Text(), original + "\n");
}

TEST(PlayPreparationPipelineTests, PublishesVerifiedCanonicalLaunchFromSavedSource)
{
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(StarterArenaProject()));
    const auto sessionRoot = StarterArenaProject().parent_path() /
        "Build/Editor/Play/preparation-contract";
    std::error_code ignored;
    std::filesystem::remove_all(sessionRoot, ignored);

    std::vector<std::string> progress;
    const auto lockPath = StarterArenaProject().parent_path() / "SagaPackage.lock";
    const auto lockBefore = ReadFile(lockPath);
    const auto sourceSnapshot = SagaEditor::CapturePlaySourceSnapshot(workspace, "preparation-contract");
    ASSERT_TRUE(sourceSnapshot.Succeeded());
    EXPECT_NE(std::find_if(sourceSnapshot.snapshot->sources.begin(),
                           sourceSnapshot.snapshot->sources.end(), [](const auto& source)
        { return source.projectRelativePath == std::filesystem::path("Assets/Door.asset.json"); }),
        sourceSnapshot.snapshot->sources.end());
    std::map<std::filesystem::path, std::string> sourceBytes;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             StarterArenaProject().parent_path()))
    {
        if (!entry.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(
            entry.path(), StarterArenaProject().parent_path());
        if (*relative.begin() == std::filesystem::path("Build")) continue;
        sourceBytes.emplace(entry.path(), ReadFile(entry.path()));
    }
    const auto result = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace,
        {"preparation-contract", RuntimeExecutable(), {},
         [&](std::string_view stage) { progress.emplace_back(stage); }, {}});

    ASSERT_TRUE(result.passed) << (result.diagnostics.empty() ? "" :
        result.diagnostics.front().code + ": " + result.diagnostics.front().message);
    EXPECT_FALSE(result.cancelled);
    EXPECT_FALSE(result.stale);
    EXPECT_TRUE(std::filesystem::is_regular_file(result.canonicalManifestPath));
    EXPECT_TRUE(std::filesystem::is_regular_file(result.localEnvelopePath));
    EXPECT_TRUE(SagaRuntime::Play::IsSha256(result.canonicalManifestHash));
    EXPECT_EQ(progress.back(), "Prepared");
    EXPECT_NE(std::find(progress.begin(), progress.end(), "ValidateProject"), progress.end());
    EXPECT_NE(std::find(progress.begin(), progress.end(), "VerifyingPackages"), progress.end());
    EXPECT_EQ(std::find(progress.begin(), progress.end(), "ResolvingPackages"), progress.end());
    EXPECT_EQ(ReadFile(lockPath), lockBefore);
    for (const auto& [path, bytes] : sourceBytes)
        EXPECT_EQ(ReadFile(path), bytes) << path;
    for (const auto& [name, hash] : result.canonicalHashes)
    {
        EXPECT_FALSE(name.empty());
        EXPECT_TRUE(SagaRuntime::Play::IsSha256(hash)) << name << "=" << hash;
    }
}

TEST(PlayPreparationPipelineTests, LockMutationAtPublicationBarrierCannotReplaceLaunch)
{
    const auto root = std::filesystem::temp_directory_path() / ("saga-play-lock-race-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::copy(StarterArenaProject().parent_path(), root,
        std::filesystem::copy_options::recursive);
    const auto projectPath = root / "StarterArena.sagaproj";
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(projectPath));
    const auto launchPath = root / "Build/Editor/Play/lock-race/launch/runtime-launch-v1.json";
    std::filesystem::create_directories(launchPath.parent_path());
    const std::string previous = "previous-current-launch\n";
    std::ofstream(launchPath, std::ios::binary | std::ios::trunc) << previous;
    const auto lockPath = root / "SagaPackage.lock";

    SagaEditor::PlayPreparationRequest request{"lock-race", RuntimeExecutable(), {}, {}, {}};
    request.beforeLaunchPublication = [&]
    {
        std::ofstream(lockPath, std::ios::binary | std::ios::app) << " ";
    };
    const auto result = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace, request);

    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.stale);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "Play.Prepare.StaleSource");
    EXPECT_EQ(ReadFile(launchPath), previous);
    std::filesystem::remove_all(root);
}

TEST(PlayPreparationPipelineTests, RuntimeBridgeMutationAtPublicationBarrierCannotReplaceLaunch)
{
    const auto root = std::filesystem::temp_directory_path() / ("saga-play-bridge-race-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::copy(StarterArenaProject().parent_path(), root,
        std::filesystem::copy_options::recursive);
    SagaEditor::EditorDocumentWorkspace workspace;
    ASSERT_TRUE(workspace.OpenProject(root / "StarterArena.sagaproj"));
    const auto launchPath = root / "Build/Editor/Play/bridge-race/launch/runtime-launch-v1.json";
    std::filesystem::create_directories(launchPath.parent_path());
    const std::string previous = "previous-current-launch\n";
    std::ofstream(launchPath, std::ios::binary | std::ios::trunc) << previous;
    std::ifstream currentInput(root / "Build/Packages/manifests/current.json");
    const auto current = nlohmann::json::parse(currentInput);
    const auto generation = root / "Build/Packages/generations" /
        current.at("generationId").get<std::string>();
    std::ifstream referencesInput(generation / "csharp_references_v1.json");
    const auto references = nlohmann::json::parse(referencesInput);
    const auto bridgePath = generation /
        references.at("references").at(0).at("resolvedAssemblyLogicalPath").get<std::string>();

    SagaEditor::PlayPreparationRequest request{"bridge-race", RuntimeExecutable(), {}, {}, {}};
    request.beforeLaunchPublication = [&]
    {
        std::ofstream(bridgePath, std::ios::binary | std::ios::app) << "x";
    };
    const auto result = SagaEditor::PlayPreparationPipeline{}.Prepare(workspace, request);

    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.stale);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "Play.Prepare.StaleDependency");
    EXPECT_EQ(ReadFile(launchPath), previous);
    std::filesystem::remove_all(root);
}

TEST(PlaySourceSnapshotTests, CloseReplacementAndDocumentDestructionInvalidateFenceSafely)
{
    const auto [root, client] = SnapshotFixture();
    SagaEditor::PlaySourceSnapshot snapshot;
    {
        SagaEditor::EditorDocumentWorkspace workspace(client);
        ASSERT_TRUE(workspace.OpenProject(root / "Project.sagaproj"));
        const auto captured = SagaEditor::CapturePlaySourceSnapshot(workspace, "lifetime");
        ASSERT_TRUE(captured.Succeeded());
        snapshot = *captured.snapshot;
        workspace.Close();
        ASSERT_FALSE(SagaEditor::VerifyPlaySourceSnapshot(snapshot).empty());
        ASSERT_TRUE(workspace.OpenProject(root / "Project.sagaproj"));
    }
    EXPECT_FALSE(SagaEditor::VerifyPlaySourceSnapshot(snapshot).empty());
    std::filesystem::remove_all(root);
}

TEST(PlaySourceSnapshotTests, MutationAndSaveAllInvalidateCapturedWorkspaceGeneration)
{
    const auto [root, client] = SnapshotFixture();
    SagaEditor::EditorDocumentWorkspace workspace(client);
    ASSERT_TRUE(workspace.OpenProject(root / "Project.sagaproj"));
    const auto captured = SagaEditor::CapturePlaySourceSnapshot(workspace, "save-all");
    ASSERT_TRUE(captured.Succeeded());
    ASSERT_TRUE(workspace.Scripts().front()->ReplaceText("public class Changed {}\n"));
    ASSERT_TRUE(workspace.SaveAll());
    const auto stale = SagaEditor::VerifyPlaySourceSnapshot(*captured.snapshot);
    ASSERT_FALSE(stale.empty());
    EXPECT_EQ(stale.front().code, "Play.Prepare.StaleSource");
    std::filesystem::remove_all(root);
}
