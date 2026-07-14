/// @file ShortcutCustomizationFeedback.h
/// @brief Qt-free notification formatting for Safe Shortcut Preferences operations.

#pragma once

#include "SagaEditor/Customization/ShortcutCustomizationSession.h"
#include "SagaEditor/Notifications/EditorNotificationCenter.h"

namespace SagaEditor
{

/// Stable operation names emitted by the Safe Shortcut Preferences surface.
enum class ShortcutCustomizationFeedbackOperation
{
    Save,
    Reset,
    Reload,
    Import,
    Export
};

/// Source id used for transient shortcut customization notifications.
inline constexpr const char* kShortcutCustomizationNotificationSource =
    "editor.customization.shortcuts";

/// Build a deterministic user-facing notification for a shortcut result.
[[nodiscard]] EditorNotification MakeShortcutCustomizationNotification(
    ShortcutCustomizationFeedbackOperation operation,
    const ShortcutCustomizationSessionResult& result,
    bool restartRequired);

} // namespace SagaEditor
