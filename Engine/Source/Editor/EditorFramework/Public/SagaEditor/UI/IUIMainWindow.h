// SPDX-License-Identifier: MPL-2.0
/// @file IUIMainWindow.h
/// @brief Abstract main window — title, visibility, menus, status, and docking host.

#pragma once

#include "SagaEditor/Shell/ShellLayout.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QWidget;

namespace SagaEditor
{
// ─── Dock Area Hint ───────────────────────────────────────────────────────────

/// Typed editor-shell dock placement hint interpreted by the Qt composition.
enum class UIDockArea : uint8_t
{
    Left,
    Right,
    Top,
    Bottom,
    Center,   ///< Set as the central widget (not a dock).
    Floating, ///< Open as a floating window.
};

// ─── Main Window Interface ────────────────────────────────────────────────────

/// Editor-shell window ownership and test seam for chrome, docking, and events.
/// QtUIMainWindow is the production implementation composed by EditorQt.
class IUIMainWindow
{
public:
    using CloseCallback = std::function<bool()>;
    using CommandCallback = std::function<void(const std::string& commandId)>;
    using WorkflowStateCallback = std::function<void()>;

    virtual ~IUIMainWindow() = default;

    // ─── Visibility & Sizing ──────────────────────────────────────────────────

    virtual void Show()                          = 0;
    virtual void ShowMaximized()                 = 0;
    virtual void Hide()                          = 0;
    virtual void Close()                         = 0;

    virtual void SetTitle(const std::string& title) = 0;
    virtual void SetSize(int width, int height)     = 0;

    [[nodiscard]] virtual int  GetWidth()        const noexcept = 0;
    [[nodiscard]] virtual int  GetHeight()       const noexcept = 0;

    // ─── Chrome ───────────────────────────────────────────────────────────────

    /// Rebuild the entire menu bar from a ShellLayout descriptor.
    virtual void ApplyShellLayout(const ShellLayout& layout) = 0;

    /// Write a message to the status bar.
    virtual void SetStatusMessage(const std::string& message) = 0;

    /// Synchronise every native action representing a command.
    virtual void SetCommandEnabled(const std::string&, bool) {}
    virtual void SetCommandChecked(const std::string&, bool) {}

    /// Restore the backend's canonical dock placement.
    virtual void ResetWorkspaceLayout() {}

    // ─── Panel Docking ────────────────────────────────────────────────────────

    /// Dock a panel widget into the given area. The window takes display
    /// ownership of the widget.
    virtual void DockPanel(QWidget*           widget,
                           const std::string& panelId,
                           const std::string& title,
                           UIDockArea         area)         = 0;

    /// Remove a docked panel by its PanelId.
    virtual void UndockPanel(const std::string& panelId)   = 0;

    /// Raise / show a docked panel by its PanelId.
    virtual void FocusPanel(const std::string& panelId)    = 0;

    /// Show or hide a docked panel by its PanelId without destroying it.
    virtual void SetPanelVisible(const std::string& panelId,
                                 bool               visible) = 0;

    // ─── Layout Persistence ───────────────────────────────────────────────────

    /// Serialise the current window and dock arrangement to a byte blob.
    [[nodiscard]] virtual std::vector<uint8_t> SaveState()                        const = 0;

    /// Restore a previously saved arrangement. Returns false on mismatch.
    [[nodiscard]] virtual bool                 RestoreState(const std::vector<uint8_t>& state) = 0;

    // ─── Events ───────────────────────────────────────────────────────────────

    virtual void SetOnClose(CloseCallback cb) = 0;

    /// Called when a menu or toolbar action carrying a command id is triggered.
    virtual void SetOnCommand(CommandCallback cb) = 0;

    /// Lightweight callback driven by the native UI event loop.
    virtual void SetOnWorkflowStatePoll(WorkflowStateCallback) {}
};

} // namespace SagaEditor
