#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct AuthoringDiagnostic
{
    std::string code;
    std::string message;
    std::string path;
};

struct ProjectReadinessPathReference
{
    std::string id;
    std::filesystem::path path;
};

struct ProjectReadinessView
{
    bool ok = false;
    std::string projectId;
    std::string displayName;
    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;
    std::filesystem::path diagnosticsPath;
    std::filesystem::path generatedReportsPath;
    std::vector<ProjectReadinessPathReference> scriptFolders;
    std::vector<ProjectReadinessPathReference> launchProfiles;
    std::vector<ProjectReadinessPathReference> packageProfiles;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ProjectReadinessView LoadProjectReadinessView(
    const std::filesystem::path& manifestPath);

} // namespace SagaEditor::Authoring
