/// @file EditorProjectInspectionTests.cpp
/// @brief Verifies the Editor-owned schema-2 project inspection contract.

#include "SagaEditor/Host/EditorProjectInspection.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

namespace fs = std::filesystem;

[[nodiscard]] bool ContainsKeyRecursively(const nlohmann::json& value,
                                          const std::string& key)
{
    if (value.is_object())
    {
        if (value.contains(key)) return true;
        for (const auto& [name, child] : value.items())
        {
            (void)name;
            if (ContainsKeyRecursively(child, key)) return true;
        }
    }
    else if (value.is_array())
    {
        for (const auto& child : value)
        {
            if (ContainsKeyRecursively(child, key)) return true;
        }
    }
    return false;
}

TEST(EditorProjectInspectionTests, WritesCleanSchemaTwoTypedActions)
{
    const fs::path reportPath = fs::temp_directory_path() /
        "saga_editor_schema_two_inspection.json";
    std::error_code error;
    fs::remove(reportPath, error);

    SagaEditor::EditorProjectInspectionRequest request;
    request.projectPath = fs::path(SAGA_SOURCE_ROOT) / "Samples" /
        "StarterArena" / "StarterArena.sagaproj";
    request.reportPath = reportPath;

    const SagaEditor::EditorProjectInspectionResult result =
        SagaEditor::InspectEditorProject(request);

    ASSERT_TRUE(result.ok) << result.error;
    std::ifstream input(reportPath);
    ASSERT_TRUE(input.is_open());
    const nlohmann::json report = nlohmann::json::parse(input);

    EXPECT_EQ(report.at("schemaVersion"), 2);
    EXPECT_EQ(report.at("operation"), "inspect-project");
    EXPECT_FALSE(report.at("verified").get<bool>());
    ASSERT_TRUE(report.at("actions").is_array());
    ASSERT_FALSE(report.at("actions").empty());
    for (const auto& action : report.at("actions"))
    {
        EXPECT_TRUE(action.contains("id"));
        EXPECT_TRUE(action.contains("kind"));
        EXPECT_TRUE(action.contains("owner"));
        EXPECT_TRUE(action.contains("availability"));
    }
    EXPECT_FALSE(ContainsKeyRecursively(report, "command"));

    const std::string serialized = report.dump();
    EXPECT_EQ(serialized.find("0.0.9"), std::string::npos);
    EXPECT_EQ(serialized.find("MultiplayerSandboxHeadless"), std::string::npos);
    EXPECT_EQ(serialized.find("server-smoke"), std::string::npos);
    EXPECT_EQ(serialized.find("SagaServer"), std::string::npos);
}

} // namespace
