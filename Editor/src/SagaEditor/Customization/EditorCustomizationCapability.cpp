/// @file EditorCustomizationCapability.cpp
/// @brief Editor customization capability derivation.

#include "SagaEditor/Customization/EditorCustomizationCapability.h"

#include "SagaEditor/Composition/ResolvedEditorCompositionSnapshot.h"

#include <unordered_set>
#include <utility>

namespace SagaEditor
{
namespace
{

[[nodiscard]] std::unordered_set<std::string> MakeSet(
    const std::vector<std::string>& values)
{
    return std::unordered_set<std::string>(values.begin(), values.end());
}

[[nodiscard]] EditorCustomizationLockReason ResolvePanelLockReason(
    const ResolvedPanelDefinition& panel,
    bool implementationAvailable)
{
    if (panel.definition.internalOnly)
    {
        return EditorCustomizationLockReason::InternalOnly;
    }
    if (!implementationAvailable)
    {
        return EditorCustomizationLockReason::UnavailableImplementation;
    }
    return EditorCustomizationLockReason::None;
}

} // namespace

EditorCustomizationCapabilityModel BuildEditorCustomizationCapabilities(
    const ResolvedEditorCompositionSnapshot* snapshot,
    const EditorCustomizationAvailability& availability)
{
    EditorCustomizationCapabilityModel model;
    model.activeWorkspaceId = availability.activeWorkspaceId;

    if (snapshot == nullptr || !snapshot->isUsable)
    {
        AddCustomizationDiagnostic(model.diagnostics,
                                   EditorCustomizationDiagnosticSeverity::Blocker,
                                   "Customization.NoCompositionSnapshot",
                                   "No usable resolved editor composition snapshot is available.");
        return model;
    }

    model.hasUsableSnapshot = true;
    model.compositionId = snapshot->compositionId;
    model.artifactHash = snapshot->artifactHash;

    const std::unordered_set<std::string> registeredPanels =
        MakeSet(availability.registeredPanelIds);
    const std::unordered_set<std::string> availableActions =
        MakeSet(availability.availableActionIds);
    const std::unordered_set<std::string> shortcutAssignableActions =
        MakeSet(availability.shortcutAssignableActionIds);

    for (const ResolvedPanelDefinition& panel : snapshot->panels)
    {
        PanelCustomizationCapability capability;
        capability.id = panel.definition.id;
        capability.displayName = panel.definition.displayName;
        capability.defaultVisible = panel.definition.defaultVisible;
        capability.currentVisible = panel.visible;
        capability.implementationAvailable =
            registeredPanels.contains(panel.definition.id);
        capability.internalOnly = panel.definition.internalOnly;
        capability.productSafe = !panel.definition.internalOnly;
        capability.lockedReason =
            ResolvePanelLockReason(panel, capability.implementationAvailable);
        capability.editable =
            capability.productSafe &&
            capability.lockedReason == EditorCustomizationLockReason::None;

        if (capability.lockedReason ==
            EditorCustomizationLockReason::UnavailableImplementation)
        {
            AddCustomizationDiagnostic(
                model.diagnostics,
                EditorCustomizationDiagnosticSeverity::Warning,
                "Customization.UnavailablePanelImplementation",
                "Panel customization is locked because the panel has no registered implementation.",
                capability.id);
        }

        model.panels.push_back(std::move(capability));
    }

    for (const ActionDefinition& action : snapshot->actions)
    {
        ActionCustomizationCapability capability;
        capability.actionId = action.id;
        capability.displayName = action.displayName;
        capability.commandImplementationAvailable =
            availableActions.contains(action.id);
        capability.shortcutAssignable =
            shortcutAssignableActions.contains(action.id);
        capability.internalOnly = action.internalOnly;
        capability.productSafe = action.safeInProduct && !action.internalOnly;
        model.actions.push_back(std::move(capability));
    }

    for (const WorkspaceProfileDefinition& workspace : snapshot->workspaces)
    {
        WorkspaceCustomizationCapability capability;
        capability.workspaceId = workspace.id;
        capability.displayName = workspace.displayName;
        capability.active =
            !model.activeWorkspaceId.empty() &&
            model.activeWorkspaceId == workspace.id;
        model.workspaces.push_back(std::move(capability));
    }

    return model;
}

} // namespace SagaEditor
