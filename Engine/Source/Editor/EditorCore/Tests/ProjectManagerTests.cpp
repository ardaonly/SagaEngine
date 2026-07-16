/// @file ProjectManagerTests.cpp
/// @brief Focused tests for editor-local project session state.

#include "SagaEditor/Project/ProjectManager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace fs = std::filesystem;

[[nodiscard]] fs::path MakeTempRoot(const std::string& name)
{
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() /
        ("saga_project_manager_" + name + "_" + std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

} // namespace

TEST(ProjectManagerTests, OpensProjectDirectoryAndReadsManifestMetadata)
{
    const fs::path root = MakeTempRoot("manifest");
    WriteFile(root / "Project.sagaproj",
              R"({
                "schemaVersion": 0,
                "projectId": "saga.test",
                "displayName": "Test Saga Project",
                "engineCompatibility": { "targetVersion": "0.0.11" }
              })");

    SagaEditor::ProjectManager manager;
    int changed = 0;
    manager.SetOnProjectChanged([&changed]() { ++changed; });

    ASSERT_TRUE(manager.OpenProject(root.string()));

    EXPECT_TRUE(manager.IsProjectOpen());
    EXPECT_EQ(manager.GetCurrentProject().name, "Test Saga Project");
    EXPECT_EQ(fs::path(manager.GetCurrentProject().rootPath), fs::absolute(root));
    EXPECT_EQ(manager.GetCurrentProject().engineVersion, "0.0.11");
    EXPECT_EQ(changed, 1);

    manager.CloseProject();

    EXPECT_FALSE(manager.IsProjectOpen());
    EXPECT_TRUE(manager.GetCurrentProject().name.empty());
    EXPECT_TRUE(manager.GetCurrentProject().rootPath.empty());
    EXPECT_TRUE(manager.GetCurrentProject().engineVersion.empty());
    EXPECT_EQ(changed, 2);

    fs::remove_all(root);
}

TEST(ProjectManagerTests, RejectsDirectoryWithoutCanonicalManifest)
{
    const fs::path root = MakeTempRoot("plain_workspace");

    SagaEditor::ProjectManager manager;

    EXPECT_FALSE(manager.OpenProject(root.string()));
    EXPECT_FALSE(manager.IsProjectOpen());

    fs::remove_all(root);
}

TEST(ProjectManagerTests, RejectsMissingPathWithoutReplacingOpenProject)
{
    const fs::path root = MakeTempRoot("valid");
    const fs::path missing = root.parent_path() / (root.filename().string() + "_missing");
    WriteFile(root / "Valid.sagaproj",
              R"({"schemaVersion":0,"projectId":"valid","displayName":"Valid"})");

    SagaEditor::ProjectManager manager;
    int changed = 0;
    manager.SetOnProjectChanged([&changed]() { ++changed; });
    ASSERT_TRUE(manager.OpenProject(root.string()));

    EXPECT_FALSE(manager.OpenProject(missing.string()));

    EXPECT_TRUE(manager.IsProjectOpen());
    EXPECT_EQ(manager.GetCurrentProject().name, "Valid");
    EXPECT_EQ(changed, 1);

    fs::remove_all(root);
}

TEST(ProjectManagerTests, RejectsInvalidManifestWithoutOpeningProject)
{
    const fs::path root = MakeTempRoot("invalid_manifest");
    WriteFile(root / "Invalid.sagaproj", "{ invalid json");

    SagaEditor::ProjectManager manager;
    int changed = 0;
    manager.SetOnProjectChanged([&changed]() { ++changed; });

    EXPECT_FALSE(manager.OpenProject(root.string()));

    EXPECT_FALSE(manager.IsProjectOpen());
    EXPECT_TRUE(manager.GetCurrentProject().name.empty());
    EXPECT_EQ(changed, 0);

    fs::remove_all(root);
}
