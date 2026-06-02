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

struct TechnicalPreviewPathReference
{
    std::string id;
    std::filesystem::path path;
};

struct TechnicalPreviewProjectView
{
    bool ok = false;
    std::string projectId;
    std::string displayName;
    std::filesystem::path projectRoot;
    std::filesystem::path manifestPath;
    std::filesystem::path diagnosticsPath;
    std::filesystem::path generatedReportsPath;
    std::vector<TechnicalPreviewPathReference> scriptFolders;
    std::vector<TechnicalPreviewPathReference> launchProfiles;
    std::vector<TechnicalPreviewPathReference> packageProfiles;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] TechnicalPreviewProjectView LoadTechnicalPreviewProjectView(
    const std::filesystem::path& manifestPath);

} // namespace SagaEditor::Authoring
