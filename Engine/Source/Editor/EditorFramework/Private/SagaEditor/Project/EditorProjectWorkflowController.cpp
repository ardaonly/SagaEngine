// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Project/EditorProjectWorkflowController.h"

#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <utility>

namespace SagaEditor
{

EditorProjectWorkflowController::EditorProjectWorkflowController(
    EditorDocumentWorkspace& workspace,
    ProjectWorkflowPrompts prompts)
    : workspace_(workspace)
    , prompts_(std::move(prompts))
{}

ProjectWorkflowOutcome
EditorProjectWorkflowController::PrepareWorkspaceReplacement()
{
    const auto dirty = workspace_.DirtyDocumentIds();
    if (dirty.empty())
    {
        return ProjectWorkflowOutcome::Completed;
    }
    if (!prompts_.confirmDirtyDocuments)
    {
        return ProjectWorkflowOutcome::Cancelled;
    }

    switch (prompts_.confirmDirtyDocuments(dirty))
    {
    case DirtyDocumentDecision::SaveAll:
        return workspace_.SaveAll() ?
            ProjectWorkflowOutcome::Completed :
            ProjectWorkflowOutcome::Failed;
    case DirtyDocumentDecision::Discard:
        return ProjectWorkflowOutcome::Completed;
    case DirtyDocumentDecision::Cancel:
        return ProjectWorkflowOutcome::Cancelled;
    }
    return ProjectWorkflowOutcome::Cancelled;
}

ProjectWorkflowOutcome EditorProjectWorkflowController::CanClose()
{
    return PrepareWorkspaceReplacement();
}

ProjectWorkflowOutcome EditorProjectWorkflowController::OpenProjectPrompted()
{
    if (!prompts_.chooseExistingProject)
    {
        return ProjectWorkflowOutcome::Cancelled;
    }
    const auto selected = prompts_.chooseExistingProject();
    if (!selected)
    {
        return ProjectWorkflowOutcome::Cancelled;
    }
    const auto readiness = PrepareWorkspaceReplacement();
    if (readiness != ProjectWorkflowOutcome::Completed)
    {
        return readiness;
    }
    return workspace_.OpenProject(*selected) ?
        ProjectWorkflowOutcome::Completed :
        ProjectWorkflowOutcome::Failed;
}

ProjectWorkflowOutcome
EditorProjectWorkflowController::CreateStarterArenaPrompted()
{
    if (!prompts_.requestNewProject)
    {
        return ProjectWorkflowOutcome::Cancelled;
    }
    const auto prompt = prompts_.requestNewProject();
    if (!prompt)
    {
        return ProjectWorkflowOutcome::Cancelled;
    }
    const auto readiness = PrepareWorkspaceReplacement();
    if (readiness != ProjectWorkflowOutcome::Completed)
    {
        return readiness;
    }
    const auto request = MakeStarterArenaTemplateRequest(
        prompt->destination, prompt->displayName);
    return workspace_.CreateStarterArena(request) ?
        ProjectWorkflowOutcome::Completed :
        ProjectWorkflowOutcome::Failed;
}

} // namespace SagaEditor
