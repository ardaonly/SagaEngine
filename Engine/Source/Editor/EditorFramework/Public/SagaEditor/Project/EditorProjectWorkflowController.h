// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

class EditorDocumentWorkspace;

struct ProjectCreationPrompt
{
    std::string displayName;
    std::filesystem::path destination;
    ProjectTemplateId templateId{};
};

enum class DirtyDocumentDecision
{
    SaveAll,
    Discard,
    Cancel,
};

enum class ProjectWorkflowOutcome
{
    Completed,
    Cancelled,
    Failed,
};

struct ProjectWorkflowPrompts
{
    std::function<std::optional<std::filesystem::path>()> chooseExistingProject;
    std::function<std::optional<ProjectCreationPrompt>()> requestNewProject;
    std::function<DirtyDocumentDecision(const std::vector<std::string>&)>
        confirmDirtyDocuments;
};

/// Coordinates project replacement decisions without owning UI or toolkit state.
class EditorProjectWorkflowController
{
public:
    EditorProjectWorkflowController(EditorDocumentWorkspace& workspace,
                                    ProjectWorkflowPrompts prompts);

    [[nodiscard]] ProjectWorkflowOutcome CanClose();
    [[nodiscard]] ProjectWorkflowOutcome OpenProjectPrompted();
    [[nodiscard]] ProjectWorkflowOutcome CreateStarterArenaPrompted();
    [[nodiscard]] ProjectWorkflowOutcome CreateProjectPrompted();

private:
    [[nodiscard]] ProjectWorkflowOutcome PrepareWorkspaceReplacement();

    EditorDocumentWorkspace& workspace_;
    ProjectWorkflowPrompts prompts_;
};

} // namespace SagaEditor
