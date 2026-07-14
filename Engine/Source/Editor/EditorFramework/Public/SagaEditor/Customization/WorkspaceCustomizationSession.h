/// @file WorkspaceCustomizationSession.h
/// @brief Host-owned session for safe workspace customization.

#pragma once

#include "SagaEditor/Customization/EditorCustomizationCapability.h"
#include "SagaEditor/Customization/EditorCustomizationDiagnostics.h"
#include "SagaEditor/Customization/EditorCustomizationOverlay.h"
#include "SagaEditor/Customization/WorkspaceCustomizationController.h"
#include "SagaEditor/Customization/WorkspaceCustomizationModel.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

class IEditorDiagnosticsService;
struct ResolvedEditorCompositionSnapshot;

/// Startup inputs for a host-owned workspace customization session.
struct WorkspaceCustomizationSessionConfig
{
    std::filesystem::path workspaceRoot;              ///< Resolved workspace root.
    const ResolvedEditorCompositionSnapshot* snapshot = nullptr; ///< Immutable composition snapshot.
    EditorCustomizationAvailability availability;     ///< Runtime availability from shell/commands.
    IEditorDiagnosticsService* diagnosticsService = nullptr; ///< Optional diagnostics bridge.
    std::string overlayId = "user.workspace.default"; ///< Stable overlay id for saves.
};

/// Result for save/reset/reload operations.
struct WorkspaceCustomizationSessionResult
{
    bool succeeded = false;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
};

/// Integrates the safe customization controller with host state and persistence.
class WorkspaceCustomizationSession
{
public:
    WorkspaceCustomizationSession();
    ~WorkspaceCustomizationSession();

    /// Initialize the session and load the default user overlay when present.
    [[nodiscard]] bool Init(WorkspaceCustomizationSessionConfig config);

    /// Clear controller, loaded overlay, diagnostics, and path state.
    void Shutdown();

    /// Refresh runtime availability after shell panel/command registration changes.
    void RefreshAvailability(EditorCustomizationAvailability availability);

    /// Return the current UI-ready model.
    [[nodiscard]] const WorkspaceCustomizationModel& GetModel() const noexcept;

    /// Return the active controller, or nullptr before initialization.
    [[nodiscard]] WorkspaceCustomizationController* GetController() noexcept;
    [[nodiscard]] const WorkspaceCustomizationController* GetController() const noexcept;

    /// Return the default overlay path used by this session.
    [[nodiscard]] const std::filesystem::path& OverlayPath() const noexcept;

    /// Return collected session and controller diagnostics.
    [[nodiscard]] const std::vector<EditorCustomizationDiagnostic>&
    Diagnostics() const noexcept;

    /// Save the current safe overlay delta to the session overlay path.
    [[nodiscard]] WorkspaceCustomizationSessionResult SaveOverlay();

    /// Delete the session overlay file and clear pending customization deltas.
    [[nodiscard]] WorkspaceCustomizationSessionResult ResetOverlay();

    /// Reload the session overlay from disk and rebuild the controller.
    [[nodiscard]] WorkspaceCustomizationSessionResult ReloadOverlay();

    /// Export the current safe workspace overlay delta to a user-selected path.
    [[nodiscard]] WorkspaceCustomizationSessionResult ExportOverlay(
        const std::filesystem::path& path);

    /// Import a safe workspace overlay from a user-selected path.
    [[nodiscard]] WorkspaceCustomizationSessionResult ImportOverlay(
        const std::filesystem::path& path);

private:
    void LoadDefaultOverlay();
    void RebuildController();
    void RefreshDiagnosticsSnapshot();
    void PublishDiagnostics() const;
    void AddDiagnostic(EditorCustomizationDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       std::string targetId = {},
                       std::string path = {});

    std::filesystem::path m_workspaceRoot;
    std::filesystem::path m_overlayPath;
    const ResolvedEditorCompositionSnapshot* m_snapshot = nullptr;
    IEditorDiagnosticsService* m_diagnosticsService = nullptr;
    EditorCustomizationAvailability m_availability;
    std::optional<EditorCustomizationOverlay> m_loadedOverlay;
    std::unique_ptr<WorkspaceCustomizationController> m_controller;
    WorkspaceCustomizationModel m_emptyModel;
    std::vector<EditorCustomizationDiagnostic> m_sessionDiagnostics;
    std::vector<EditorCustomizationDiagnostic> m_diagnosticsSnapshot;
    std::string m_overlayId = "user.workspace.default";
};

} // namespace SagaEditor
