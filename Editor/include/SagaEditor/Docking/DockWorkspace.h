/// @file DockWorkspace.h
/// @brief Panel docking façade — thin wrapper around IUIMainWindow's dock API.

#pragma once

#include "SagaEditor/UI/IUIMainWindow.h"
#include <string>

namespace SagaEditor
{

class IPanel;

// ─── Dock Workspace ───────────────────────────────────────────────────────────

/// Convenience layer that translates IPanel operations into IUIMainWindow dock
/// calls. Higher-level code (LayoutSerializer, ExtensionHost) can use this
/// instead of calling IUIMainWindow::DockPanel directly.
/// No Qt type appears in this header.
class DockWorkspace
{
public:
    explicit DockWorkspace(IUIMainWindow& window);

    // ─── Panel Management ─────────────────────────────────────────────────────

    /// Dock a panel into the given area; panel must outlive this call.
    void AddPanel(IPanel* panel, UIDockArea area);

    /// Remove a docked panel by PanelId (does not delete the panel object).
    void RemovePanel(const std::string& panelId);

    /// Bring a panel's dock tab to front.
    void FocusPanel(const std::string& panelId);

    /// Drop the current dock arrangement and rebuild it from the editor's
    /// hard-coded default placement. Used by `DockLayoutManager::ResetToDefault`
    /// when the user picks "Reset Layout" from the View menu, and by tests
    /// that want to start each case from a known baseline. The current
    /// implementation undocks every dock the workspace knows about; the
    /// shell re-adds its default panel set on the next layout build.
    void ResetToDefaultLayout();

    // ─── Layout Persistence ───────────────────────────────────────────────────

    /// Save the current dock arrangement to a byte blob.
    [[nodiscard]] std::vector<uint8_t> SaveState() const;

    /// Restore a previously saved arrangement. Returns false on mismatch.
    [[nodiscard]] bool RestoreState(const std::vector<uint8_t>& state);

private:
    IUIMainWindow& m_window; ///< Non-owning reference to the backend window.
};

} // namespace SagaEditor
