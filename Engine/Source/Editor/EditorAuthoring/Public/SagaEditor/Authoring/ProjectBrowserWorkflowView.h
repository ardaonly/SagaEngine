#pragma once

#include "SagaEditor/Authoring/ProjectReadinessView.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor::Authoring
{

struct ProjectBrowserArtifactLink
{
    std::string id;
    std::string kind;
    std::filesystem::path path;
    bool exists = false;
    bool openable = false;
};

struct ProjectBrowserSectionView
{
    std::string name;
    std::filesystem::path path;
    bool exists = false;
    std::string status;
    std::filesystem::path openableArtifactPath;
    std::vector<ProjectBrowserArtifactLink> artifactLinks;
    std::vector<AuthoringDiagnostic> diagnostics;
};

struct ProjectBrowserWorkflowView
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
    std::vector<ProjectBrowserSectionView> sections;
    std::vector<AuthoringDiagnostic> diagnostics;
};

[[nodiscard]] ProjectBrowserWorkflowView LoadProjectBrowserWorkflowView(
    const std::filesystem::path& manifestPath);

} // namespace SagaEditor::Authoring
