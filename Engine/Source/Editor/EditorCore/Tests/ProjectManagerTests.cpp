// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/ProjectManager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{
namespace fs = std::filesystem;

class FakeProjectToolClient final : public SagaEditor::IProjectToolClient
{
public:
    mutable int validationCount = 0;
    mutable int creationCount = 0;
    SagaEditor::ProjectToolResult validation;
    SagaEditor::ProjectToolResult creation;

    SagaEditor::ProjectToolResult Validate(const fs::path&) const override
    {
        ++validationCount;
        return validation;
    }

    SagaEditor::ProjectToolResult CreateStarterArena(
        const SagaEditor::ProjectTemplateRequest&) const override
    {
        ++creationCount;
        return creation;
    }
};

fs::path MakeTempRoot(const std::string& name)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
        ("saga_project_manager_" + name + "_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

void WriteManifest(const fs::path& path)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "{}\n";
}

SagaEditor::ProjectToolResult PassedProject(const fs::path& root,
                                             std::string name = "Test Saga Project")
{
    SagaEditor::ProjectToolResult result;
    result.passed = true;
    result.project.projectId = "808c32f2-c240-4613-b86b-f77af8c4a900";
    result.project.displayName = std::move(name);
    result.project.projectRoot = fs::absolute(root);
    result.project.manifestPath = fs::absolute(root / "Project.sagaproj");
    result.project.engineTargetVersion = "0.0.11";
    result.project.schemaVersion = 0;
    return result;
}
}

TEST(ProjectManagerTests, OpensOnlyAfterSagaProjectKitValidation)
{
    const auto root = MakeTempRoot("validated");
    WriteManifest(root / "Project.sagaproj");
    auto client = std::make_shared<FakeProjectToolClient>();
    client->validation = PassedProject(root);
    SagaEditor::ProjectManager manager(client);
    int changed = 0;
    manager.SetOnProjectChanged([&changed] { ++changed; });

    ASSERT_TRUE(manager.OpenProject(root.string()));
    EXPECT_EQ(client->validationCount, 1);
    EXPECT_EQ(manager.GetCurrentProject().name, "Test Saga Project");
    EXPECT_EQ(manager.GetCurrentProject().engineVersion, "0.0.11");
    EXPECT_EQ(manager.GetCurrentProject().manifestPath,
              fs::absolute(root / "Project.sagaproj").string());
    EXPECT_EQ(changed, 1);

    manager.CloseProject();
    EXPECT_FALSE(manager.IsProjectOpen());
    EXPECT_EQ(changed, 2);
    fs::remove_all(root);
}

TEST(ProjectManagerTests, RejectedValidationDoesNotReplaceOpenProject)
{
    const auto first = MakeTempRoot("first");
    const auto second = MakeTempRoot("second");
    WriteManifest(first / "Project.sagaproj");
    WriteManifest(second / "Project.sagaproj");
    auto client = std::make_shared<FakeProjectToolClient>();
    client->validation = PassedProject(first, "First");
    SagaEditor::ProjectManager manager(client);
    ASSERT_TRUE(manager.OpenProject(first.string()));

    client->validation = {};
    EXPECT_FALSE(manager.OpenProject(second.string()));
    EXPECT_TRUE(manager.IsProjectOpen());
    EXPECT_EQ(manager.GetCurrentProject().name, "First");
    fs::remove_all(first);
    fs::remove_all(second);
}

TEST(ProjectManagerTests, RequiresExactlyOneCanonicalManifest)
{
    const auto root = MakeTempRoot("ambiguous");
    WriteManifest(root / "First.sagaproj");
    WriteManifest(root / "Second.sagaproj");
    auto client = std::make_shared<FakeProjectToolClient>();
    client->validation = PassedProject(root);
    SagaEditor::ProjectManager manager(client);

    EXPECT_FALSE(manager.OpenProject(root.string()));
    EXPECT_EQ(client->validationCount, 0);
    fs::remove_all(root);
}

TEST(ProjectManagerTests, CreatesThenOpensMaterializedStarterArena)
{
    const auto parent = MakeTempRoot("create");
    const auto destination = parent / "Created";
    fs::create_directories(destination);
    WriteManifest(destination / "Project.sagaproj");
    auto client = std::make_shared<FakeProjectToolClient>();
    client->creation = PassedProject(destination, "Created");
    SagaEditor::ProjectManager manager(client);
    SagaEditor::ProjectTemplateRequest request;
    request.destination = destination;

    const auto result = manager.CreateStarterArena(request);
    ASSERT_TRUE(result.passed);
    EXPECT_EQ(client->creationCount, 1);
    EXPECT_EQ(client->validationCount, 0);
    EXPECT_EQ(manager.GetCurrentProject().name, "Created");
    fs::remove_all(parent);
}
