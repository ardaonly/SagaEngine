/// @file ShortcutCustomizationPanelViewModel.h
/// @brief Qt-free view model for the Safe Shortcut Preferences panel.

#pragma once

#include "SagaEditor/Customization/ShortcutCustomizationSession.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaEditor
{

/// UI-facing row for one shortcut customization capability.
struct ShortcutCustomizationActionRow
{
    std::string actionId;
    std::string displayName;
    std::string currentChord;
    std::string pendingChord;
    std::string effectiveChord;
    bool pending = false;
    bool editable = false;
    std::string lockedReason;
};

/// UI-facing diagnostic row for shortcut customization status display.
struct ShortcutCustomizationDiagnosticRow
{
    EditorCustomizationDiagnosticSeverity severity =
        EditorCustomizationDiagnosticSeverity::Info;
    std::string code;
    std::string message;
};

/// Immutable view snapshot consumed by the Shortcut Preferences UI.
struct ShortcutCustomizationPanelViewState
{
    ShortcutCustomizationStatus status =
        ShortcutCustomizationStatus::Unconfigured;
    std::string statusText;
    std::string overlayPath;
    bool dirty = false;
    bool restartRequired = false;
    bool canSave = false;
    bool canReset = false;
    std::vector<ShortcutCustomizationActionRow> actions;
    std::vector<ShortcutCustomizationDiagnosticRow> diagnostics;
};

/// Thin controller/view adapter for the Safe Shortcut Preferences panel.
class ShortcutCustomizationPanelViewModel
{
public:
    explicit ShortcutCustomizationPanelViewModel(
        ShortcutCustomizationSession& session);

    /// Return the latest projected view state.
    [[nodiscard]] ShortcutCustomizationPanelViewState GetState() const;

    /// Set a safe shortcut through the customization controller.
    [[nodiscard]] bool SetShortcut(const std::string& actionId,
                                   const std::string& chord);

    /// Clear a safe shortcut through the customization controller.
    [[nodiscard]] bool ClearShortcut(const std::string& actionId);

    /// Reset one pending shortcut override.
    [[nodiscard]] bool ResetShortcut(const std::string& actionId);

    /// Persist the current safe shortcut overlay delta.
    [[nodiscard]] ShortcutCustomizationSessionResult Save();

    /// Delete shortcut overrides while preserving other safe overlay sections.
    [[nodiscard]] ShortcutCustomizationSessionResult ResetAll();

    /// Reload the safe overlay from disk.
    [[nodiscard]] ShortcutCustomizationSessionResult Reload();

    /// Export the current safe shortcut overlay delta.
    [[nodiscard]] ShortcutCustomizationSessionResult ExportOverlay(
        const std::filesystem::path& path);

    /// Import safe shortcut overrides from disk.
    [[nodiscard]] ShortcutCustomizationSessionResult ImportOverlay(
        const std::filesystem::path& path);

private:
    ShortcutCustomizationSession* m_session = nullptr; ///< Non-owning host service.
};

/// Convert shortcut customization status to a stable display string.
[[nodiscard]] std::string ToDisplayString(ShortcutCustomizationStatus status);

} // namespace SagaEditor
