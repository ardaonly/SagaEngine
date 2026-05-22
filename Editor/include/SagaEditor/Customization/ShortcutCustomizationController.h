/// @file ShortcutCustomizationController.h
/// @brief Controller for safe shortcut customization overlay deltas.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationOverlay.h"
#include "SagaEditor/Customization/ShortcutCustomizationModel.h"

#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

struct ResolvedEditorCompositionSnapshot;

/// Inputs used to construct a safe shortcut customization controller.
struct ShortcutCustomizationControllerConfig
{
    const ResolvedEditorCompositionSnapshot* snapshot = nullptr; ///< Immutable resolved composition.
    EditorCustomizationAvailability availability;                ///< Runtime command availability inputs.
    std::optional<EditorCustomizationOverlay> initialOverlay;    ///< Existing user overlay.
    std::string overlayId = "user.workspace.default";            ///< Stable overlay id to emit.
};

/// Mutates safe shortcut overlay deltas without mutating the composition snapshot.
class ShortcutCustomizationController
{
public:
    explicit ShortcutCustomizationController(
        ShortcutCustomizationControllerConfig config);

    /// Set a shortcut override for a safe action.
    [[nodiscard]] bool SetShortcut(const std::string& actionId,
                                   const std::string& chord);

    /// Clear the shortcut for a safe action.
    [[nodiscard]] bool ClearShortcut(const std::string& actionId);

    /// Remove one pending shortcut override.
    [[nodiscard]] bool ResetShortcut(const std::string& actionId);

    /// Remove all pending shortcut customization overrides.
    void ResetAllShortcuts();

    /// Return the current read model.
    [[nodiscard]] const ShortcutCustomizationModel& GetModel() const noexcept;

    /// Return accumulated controller diagnostics.
    [[nodiscard]] const std::vector<EditorCustomizationDiagnostic>&
    GetDiagnostics() const noexcept;

    /// Validate the current pending overlay delta.
    [[nodiscard]] bool Validate();

    /// Build a safe overlay delta compatible with the existing composition resolver.
    [[nodiscard]] EditorCustomizationOverlay BuildOverlayDelta() const;

private:
    void RebuildModel();
    [[nodiscard]] ShortcutCustomizationActionState* FindActionState(
        const std::string& actionId);
    [[nodiscard]] const ActionCustomizationCapability* FindActionCapability(
        const std::string& actionId) const;
    [[nodiscard]] std::string CurrentChordForAction(
        const std::string& actionId) const;
    [[nodiscard]] bool ValidateChordForAction(const std::string& actionId,
                                              const std::string& chord);
    void AddDiagnostic(EditorCustomizationDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       std::string targetId = {});

    EditorCustomizationCapabilityModel m_capabilities;
    ShortcutCustomizationModel m_model;
    std::vector<EditorCustomizationDiagnostic> m_diagnostics;
    std::vector<ShortcutCustomizationOverride> m_pendingShortcuts;
    std::vector<LayoutCustomizationOverride> m_passthroughLayout;
    std::vector<VisibilityCustomizationOverride> m_passthroughVisibility;
    std::vector<ToolbarCustomizationOverride> m_passthroughToolbar;
    const ResolvedEditorCompositionSnapshot* m_snapshot = nullptr;
    std::string m_overlayId;
};

} // namespace SagaEditor
