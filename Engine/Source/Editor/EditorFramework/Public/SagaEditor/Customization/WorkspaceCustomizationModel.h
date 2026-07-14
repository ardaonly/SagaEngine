/// @file WorkspaceCustomizationModel.h
/// @brief UI-ready Qt-free model for safe workspace customization state.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationCapability.h"

#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

/// High-level customization model state.
enum class WorkspaceCustomizationStatus
{
    Unconfigured,
    Ready,
    Invalid
};

/// Panel row projected for future workspace customization UI.
struct WorkspaceCustomizationPanelState
{
    std::string id;
    std::string displayName;
    bool defaultVisible = false;
    bool currentVisible = false;
    std::optional<bool> pendingVisible;
    bool editable = false;
    EditorCustomizationLockReason lockedReason =
        EditorCustomizationLockReason::None;
};

/// Read model exposed by the customization controller.
struct WorkspaceCustomizationModel
{
    WorkspaceCustomizationStatus status =
        WorkspaceCustomizationStatus::Unconfigured;
    std::string compositionId;
    std::string artifactHash;
    std::string activeWorkspaceId;
    std::vector<WorkspaceCustomizationPanelState> panels;
    std::vector<ActionCustomizationCapability> actions;
    std::vector<WorkspaceCustomizationCapability> workspaces;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
    bool dirty = false;
    bool liveApplyAvailable = false;
    bool restartRequired = false;
};

} // namespace SagaEditor
