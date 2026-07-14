/// @file WorkspaceCustomizationController.h
/// @brief Controller for safe workspace customization overlay deltas.

#pragma once

#include "SagaEditor/Customization/WorkspaceCustomizationModel.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"

#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

struct ResolvedEditorCompositionSnapshot;

/// Inputs used to construct a safe workspace customization controller.
struct WorkspaceCustomizationControllerConfig
{
    const ResolvedEditorCompositionSnapshot* snapshot = nullptr; ///< Immutable resolved composition.
    EditorCustomizationAvailability availability;                ///< Runtime availability inputs.
    std::optional<EditorCustomizationOverlay> initialOverlay;    ///< Existing user overlay.
    std::string overlayId = "user.workspace.default";            ///< Stable overlay id to emit.
};

/// Mutates safe overlay deltas without mutating the resolved composition snapshot.
class WorkspaceCustomizationController
{
public:
    explicit WorkspaceCustomizationController(
        WorkspaceCustomizationControllerConfig config);

    /// Set a safe panel visibility override.
    [[nodiscard]] bool TogglePanelVisibility(const std::string& panelId,
                                             bool visible);

    /// Remove one pending panel visibility override.
    [[nodiscard]] bool ResetPanel(const std::string& panelId);

    /// Remove all pending workspace customization overrides.
    void ResetWorkspace();

    /// Return the current read model.
    [[nodiscard]] const WorkspaceCustomizationModel& GetModel() const noexcept;

    /// Return accumulated controller diagnostics.
    [[nodiscard]] const std::vector<EditorCustomizationDiagnostic>&
    GetDiagnostics() const noexcept;

    /// Validate the current pending overlay delta.
    [[nodiscard]] bool Validate();

    /// Build a safe overlay delta compatible with the existing composition resolver.
    [[nodiscard]] EditorCustomizationOverlay BuildOverlayDelta() const;

private:
    void RebuildModel();
    [[nodiscard]] WorkspaceCustomizationPanelState* FindPanelState(
        const std::string& panelId);
    [[nodiscard]] const WorkspaceCustomizationPanelState* FindPanelState(
        const std::string& panelId) const;
    [[nodiscard]] const PanelCustomizationCapability* FindPanelCapability(
        const std::string& panelId) const;
    void AddDiagnostic(EditorCustomizationDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       std::string targetId = {});

    EditorCustomizationCapabilityModel m_capabilities;
    WorkspaceCustomizationModel m_model;
    std::vector<EditorCustomizationDiagnostic> m_diagnostics;
    std::vector<VisibilityCustomizationOverride> m_pendingVisibility;
    std::vector<ShortcutCustomizationOverride> m_passthroughShortcuts;
    std::vector<ToolbarCustomizationOverride> m_passthroughToolbars;
    std::string m_overlayId;
};

} // namespace SagaEditor
