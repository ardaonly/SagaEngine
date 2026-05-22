/// @file EditorCustomizationCapability.h
/// @brief Derives safe customization capabilities from resolved editor composition snapshots.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"

#include <string>
#include <vector>

namespace SagaEditor
{

struct ResolvedEditorCompositionSnapshot;

/// Reason a customization target cannot be edited safely.
enum class EditorCustomizationLockReason
{
    None,
    NoCompositionSnapshot,
    InternalOnly,
    UnavailableImplementation,
    NotProductSafe
};

/// Implementation availability supplied by the shell or command registry boundary.
struct EditorCustomizationAvailability
{
    std::vector<std::string> registeredPanelIds;      ///< Panel ids with binary implementations.
    std::vector<std::string> availableActionIds;      ///< Action ids with command implementations.
    std::vector<std::string> shortcutAssignableActionIds; ///< Action ids users may remap.
    std::string activeWorkspaceId;                    ///< Current workspace/profile id if known.
};

/// Safe customization capability for a resolved panel.
struct PanelCustomizationCapability
{
    std::string id;
    std::string displayName;
    bool defaultVisible = false;
    bool currentVisible = false;
    bool implementationAvailable = false;
    bool internalOnly = false;
    bool productSafe = false;
    bool editable = false;
    EditorCustomizationLockReason lockedReason =
        EditorCustomizationLockReason::None;
};

/// Safe customization capability for a resolved action or command.
struct ActionCustomizationCapability
{
    std::string actionId;
    std::string displayName;
    bool commandImplementationAvailable = false;
    bool shortcutAssignable = false;
    bool internalOnly = false;
    bool productSafe = false;
};

/// Workspace capability surfaced to future customization UI.
struct WorkspaceCustomizationCapability
{
    std::string workspaceId;
    std::string displayName;
    bool active = false;
};

/// Complete capability model derived without mutating the composition snapshot.
struct EditorCustomizationCapabilityModel
{
    std::string compositionId;
    std::string artifactHash;
    std::string activeWorkspaceId;
    bool hasUsableSnapshot = false;
    std::vector<PanelCustomizationCapability> panels;
    std::vector<ActionCustomizationCapability> actions;
    std::vector<WorkspaceCustomizationCapability> workspaces;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
};

/// Build safe customization capabilities from a resolved snapshot and availability data.
[[nodiscard]] EditorCustomizationCapabilityModel BuildEditorCustomizationCapabilities(
    const ResolvedEditorCompositionSnapshot* snapshot,
    const EditorCustomizationAvailability& availability);

} // namespace SagaEditor
