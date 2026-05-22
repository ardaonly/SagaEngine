/// @file EditorCompositionShellAdapter.h
/// @brief Projects resolved editor composition snapshots into shell state.

#pragma once

#include "SagaEditor/Diagnostics/EditorDiagnostic.h"
#include "SagaEditor/Shell/ShellLayout.h"

#include <string>
#include <vector>

namespace SagaEditor
{

class CommandRegistry;
class ResolvedEditorCompositionSnapshot;
class ShortcutManager;

/// Result of converting a resolved composition snapshot into shell chrome.
struct EditorCompositionShellLayoutResult
{
    ShellLayout layout;                            ///< Layout ready for IUIMainWindow.
    std::vector<EditorDiagnostic> diagnostics;     ///< Non-fatal projection diagnostics.
    bool usedComposition = false;                  ///< True when snapshot data replaced defaults.
};

/// Converts resolved composition snapshots into shell layout and bindings.
class EditorCompositionShellAdapter
{
public:
    /// Build a ShellLayout from menus and toolbars in the resolved snapshot.
    [[nodiscard]] EditorCompositionShellLayoutResult BuildShellLayout(
        const ResolvedEditorCompositionSnapshot& snapshot,
        const CommandRegistry& commands,
        const ShellLayout& fallback) const;

    /// Replace shortcut bindings with the resolved composition shortcuts.
    [[nodiscard]] std::vector<EditorDiagnostic> ApplyShortcuts(
        const ResolvedEditorCompositionSnapshot& snapshot,
        const CommandRegistry& commands,
        ShortcutManager& shortcuts) const;

    /// Validate that resolved panel ids have registered panel implementations.
    [[nodiscard]] std::vector<EditorDiagnostic> ValidateRegisteredPanels(
        const ResolvedEditorCompositionSnapshot& snapshot,
        const std::vector<std::string>& registeredPanelIds) const;
};

} // namespace SagaEditor
