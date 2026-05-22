/// @file WorkspaceCustomizationPanelViewModel.h
/// @brief Qt-free view model for the Safe Customize Workspace panel.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationDiagnostics.h"
#include "SagaEditor/Customization/WorkspaceCustomizationModel.h"
#include "SagaEditor/Customization/WorkspaceCustomizationSession.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

/// UI-facing row for one panel customization capability.
struct WorkspaceCustomizationPanelRow
{
    std::string id;
    std::string displayName;
    bool visible = false;
    bool currentVisible = false;
    bool defaultVisible = false;
    bool pending = false;
    bool editable = false;
    std::string lockedReason;
};

/// UI-facing diagnostic row for customization status display.
struct WorkspaceCustomizationDiagnosticRow
{
    EditorCustomizationDiagnosticSeverity severity =
        EditorCustomizationDiagnosticSeverity::Info;
    std::string code;
    std::string message;
};

/// Immutable view snapshot consumed by the Customize Workspace UI.
struct WorkspaceCustomizationPanelViewState
{
    WorkspaceCustomizationStatus status =
        WorkspaceCustomizationStatus::Unconfigured;
    std::string statusText;
    std::string overlayPath;
    bool dirty = false;
    bool restartRequired = false;
    bool canSave = false;
    bool canReset = false;
    std::vector<WorkspaceCustomizationPanelRow> panels;
    std::vector<WorkspaceCustomizationDiagnosticRow> diagnostics;
};

/// Thin controller/view adapter for the Safe Customize Workspace panel.
class WorkspaceCustomizationPanelViewModel
{
public:
    explicit WorkspaceCustomizationPanelViewModel(
        WorkspaceCustomizationSession& session);

    /// Return the latest projected view state.
    [[nodiscard]] WorkspaceCustomizationPanelViewState GetState() const;

    /// Toggle a safe panel row through the customization controller.
    [[nodiscard]] bool TogglePanel(const std::string& panelId, bool visible);

    /// Persist the current safe overlay delta.
    [[nodiscard]] WorkspaceCustomizationSessionResult Save();

    /// Delete the safe overlay and clear pending customization.
    [[nodiscard]] WorkspaceCustomizationSessionResult ResetWorkspace();

    /// Reload the safe overlay from disk.
    [[nodiscard]] WorkspaceCustomizationSessionResult Reload();

    /// Export the current safe workspace overlay delta.
    [[nodiscard]] WorkspaceCustomizationSessionResult ExportOverlay(
        const std::filesystem::path& path);

    /// Import a safe workspace overlay from disk.
    [[nodiscard]] WorkspaceCustomizationSessionResult ImportOverlay(
        const std::filesystem::path& path);

private:
    WorkspaceCustomizationSession* m_session = nullptr; ///< Non-owning host service.
};

/// Convert a customization status to a stable display string.
[[nodiscard]] std::string ToDisplayString(WorkspaceCustomizationStatus status);

/// Convert a lock reason to a stable display string.
[[nodiscard]] std::string ToDisplayString(EditorCustomizationLockReason reason);

} // namespace SagaEditor
