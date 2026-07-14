/// @file WorkspaceCustomizationFeedback.cpp
/// @brief Qt-free notification formatting for Safe Customize Workspace operations.

#include "SagaEditor/Customization/WorkspaceCustomizationFeedback.h"

#include <string>

namespace SagaEditor
{
namespace
{

[[nodiscard]] const char* OperationText(
    WorkspaceCustomizationFeedbackOperation operation)
{
    switch (operation)
    {
    case WorkspaceCustomizationFeedbackOperation::Save:
        return "saved";
    case WorkspaceCustomizationFeedbackOperation::Reset:
        return "reset";
    case WorkspaceCustomizationFeedbackOperation::Reload:
        return "reloaded";
    case WorkspaceCustomizationFeedbackOperation::Import:
        return "imported";
    case WorkspaceCustomizationFeedbackOperation::Export:
        return "exported";
    }
    return "updated";
}

[[nodiscard]] std::string FirstDiagnosticDetail(
    const WorkspaceCustomizationSessionResult& result)
{
    if (result.diagnostics.empty())
    {
        return {};
    }

    const EditorCustomizationDiagnostic& diagnostic = result.diagnostics.front();
    if (!diagnostic.code.empty())
    {
        return diagnostic.code + ": " + diagnostic.message;
    }
    return diagnostic.message;
}

} // namespace

EditorNotification MakeWorkspaceCustomizationNotification(
    WorkspaceCustomizationFeedbackOperation operation,
    const WorkspaceCustomizationSessionResult& result,
    bool restartRequired)
{
    EditorNotification notification;
    notification.source = kWorkspaceCustomizationNotificationSource;
    notification.detail = FirstDiagnosticDetail(result);

    const char* operationText = OperationText(operation);
    if (result.succeeded)
    {
        notification.severity = EditorNotificationSeverity::Success;
        notification.message =
            std::string("Workspace customization ") + operationText + ".";
        if (restartRequired)
        {
            notification.message +=
                " Restart editor to apply workspace changes.";
        }
        return notification;
    }

    notification.severity = EditorNotificationSeverity::Error;
    notification.message =
        std::string("Workspace customization ") + operationText + " failed.";
    return notification;
}

} // namespace SagaEditor
