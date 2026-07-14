/// @file ShortcutCustomizationSession.h
/// @brief Host-owned session for safe shortcut customization.

#pragma once

#include "SagaEditor/Customization/ShortcutCustomizationController.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{

class IEditorDiagnosticsService;
struct ResolvedEditorCompositionSnapshot;

/// Startup inputs for a host-owned shortcut customization session.
struct ShortcutCustomizationSessionConfig
{
    std::filesystem::path workspaceRoot;              ///< Resolved workspace root.
    const ResolvedEditorCompositionSnapshot* snapshot = nullptr; ///< Immutable composition snapshot.
    EditorCustomizationAvailability availability;     ///< Runtime command availability.
    IEditorDiagnosticsService* diagnosticsService = nullptr; ///< Optional diagnostics bridge.
    std::string overlayId = "user.workspace.default"; ///< Stable overlay id for saves.
};

/// Result for shortcut customization persistence operations.
struct ShortcutCustomizationSessionResult
{
    bool succeeded = false;
    std::vector<EditorCustomizationDiagnostic> diagnostics;
};

/// Integrates shortcut customization controller with host state and persistence.
class ShortcutCustomizationSession
{
public:
    ShortcutCustomizationSession();
    ~ShortcutCustomizationSession();

    /// Initialize the session and load the shared safe overlay when present.
    [[nodiscard]] bool Init(ShortcutCustomizationSessionConfig config);

    /// Clear controller, loaded overlay, diagnostics, and path state.
    void Shutdown();

    /// Refresh runtime command availability after shell registration changes.
    void RefreshAvailability(EditorCustomizationAvailability availability);

    /// Return the current UI-ready model.
    [[nodiscard]] const ShortcutCustomizationModel& GetModel() const noexcept;

    /// Return the active controller, or nullptr before initialization.
    [[nodiscard]] ShortcutCustomizationController* GetController() noexcept;
    [[nodiscard]] const ShortcutCustomizationController* GetController() const noexcept;

    /// Return the shared overlay path used by this session.
    [[nodiscard]] const std::filesystem::path& OverlayPath() const noexcept;

    /// Return collected session and controller diagnostics.
    [[nodiscard]] const std::vector<EditorCustomizationDiagnostic>&
    Diagnostics() const noexcept;

    /// Save the current safe shortcut overlay delta.
    [[nodiscard]] ShortcutCustomizationSessionResult SaveOverlay();

    /// Clear shortcut overrides while preserving other safe overlay sections.
    [[nodiscard]] ShortcutCustomizationSessionResult ResetShortcuts();

    /// Reload the shared safe overlay from disk.
    [[nodiscard]] ShortcutCustomizationSessionResult ReloadOverlay();

    /// Export the current safe shortcut overlay delta.
    [[nodiscard]] ShortcutCustomizationSessionResult ExportOverlay(
        const std::filesystem::path& path);

    /// Import safe shortcut overrides from disk.
    [[nodiscard]] ShortcutCustomizationSessionResult ImportOverlay(
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
    std::unique_ptr<ShortcutCustomizationController> m_controller;
    ShortcutCustomizationModel m_emptyModel;
    std::vector<EditorCustomizationDiagnostic> m_sessionDiagnostics;
    std::vector<EditorCustomizationDiagnostic> m_diagnosticsSnapshot;
    std::string m_overlayId = "user.workspace.default";
};

} // namespace SagaEditor
