/// @file WorkspaceCustomizationFeedback.h
/// @brief Qt-free notification formatting for Safe Customize Workspace operations.

#pragma once

#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"

namespace SagaEditor
{

/// Stable operation names emitted by the Safe Customize Workspace surface.
enum class WorkspaceCustomizationFeedbackOperation
{
    Save,
    Reset,
    Reload,
    Import,
    Export
};

/// Source id used for transient workspace customization notifications.
inline constexpr const char* kWorkspaceCustomizationNotificationSource =
    "editor.customization.workspace";

/// Build a deterministic user-facing notification for a customization result.
[[nodiscard]] EditorNotification MakeWorkspaceCustomizationNotification(
    WorkspaceCustomizationFeedbackOperation operation,
    const WorkspaceCustomizationSessionResult& result,
    bool restartRequired);

} // namespace SagaEditor
