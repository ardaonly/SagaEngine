/// @file ShortcutCustomizationFeedback.cpp
/// @brief Qt-free notification formatting for Safe Shortcut Preferences operations.

#include "SagaEditor/Customization/ShortcutCustomizationFeedback.h"

#include <string>

namespace SagaEditor
{
namespace
{

[[nodiscard]] const char* OperationText(
    ShortcutCustomizationFeedbackOperation operation)
{
    switch (operation)
    {
    case ShortcutCustomizationFeedbackOperation::Save:
        return "saved";
    case ShortcutCustomizationFeedbackOperation::Reset:
        return "reset";
    case ShortcutCustomizationFeedbackOperation::Reload:
        return "reloaded";
    case ShortcutCustomizationFeedbackOperation::Import:
        return "imported";
    case ShortcutCustomizationFeedbackOperation::Export:
        return "exported";
    }
    return "updated";
}

[[nodiscard]] std::string FirstDiagnosticDetail(
    const ShortcutCustomizationSessionResult& result)
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

EditorNotification MakeShortcutCustomizationNotification(
    ShortcutCustomizationFeedbackOperation operation,
    const ShortcutCustomizationSessionResult& result,
    bool restartRequired)
{
    EditorNotification notification;
    notification.source = kShortcutCustomizationNotificationSource;
    notification.detail = FirstDiagnosticDetail(result);

    const char* operationText = OperationText(operation);
    if (result.succeeded)
    {
        notification.severity = EditorNotificationSeverity::Success;
        notification.message =
            std::string("Shortcut preferences ") + operationText + ".";
        if (restartRequired)
        {
            notification.message +=
                " Restart editor to apply shortcut changes.";
        }
        return notification;
    }

    notification.severity = EditorNotificationSeverity::Error;
    notification.message =
        std::string("Shortcut preferences ") + operationText + " failed.";
    return notification;
}

} // namespace SagaEditor
