/// @file ShortcutCustomizationModel.h
/// @brief UI-ready Qt-free model for safe shortcut customization state.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationCapability.h"

#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

/// High-level shortcut customization model state.
enum class ShortcutCustomizationStatus
{
    Unconfigured,
    Ready,
    Invalid
};

/// Action row projected for shortcut preferences UI.
struct ShortcutCustomizationActionState
{
    std::string actionId;
    std::string displayName;
    std::string category;
    std::string currentChord;
    std::optional<std::string> pendingChord;
    bool editable = false;
    EditorCustomizationLockReason lockedReason =
        EditorCustomizationLockReason::None;
};

/// Read model exposed by the shortcut customization controller.
struct ShortcutCustomizationModel
{
    ShortcutCustomizationStatus status =
        ShortcutCustomizationStatus::Unconfigured;
    std::string compositionId;
    std::string artifactHash;
    std::vector<ShortcutCustomizationActionState> actions;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
    bool dirty = false;
    bool liveApplyAvailable = false;
    bool restartRequired = false;
};

} // namespace SagaEditor
