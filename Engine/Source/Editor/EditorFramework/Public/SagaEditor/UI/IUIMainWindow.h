/// @file IUIMainWindow.h
/// @brief Abstract main window — title, visibility, menus, status, and docking host.

#pragma once

#include "SagaEditor/Shell/ShellLayout.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace SagaEditor
{
struct NewProjectPrompt
{
    std::string displayName;
    std::string destination;
};

enum class UnsavedDocumentsDecision : uint8_t
{
    SaveAll,
    Discard,
    Cancel,
};

// ─── Dock Area Hint ───────────────────────────────────────────────────────────

/// Framework-agnostic dock placement hint — maps to Qt::DockWidgetArea on the
/// Qt backend and to whatever the next backend uses.
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

/// Wraps the framework's top-level window (e.g. QMainWindow).
/// The editor shell talks to this interface; Qt stays behind the backend.
class IUIMainWindow
{
public:
    using CloseCallback = std::function<bool()>;
    using CommandCallback = std::function<void(const std::string& commandId)>;

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
    [[nodiscard]] virtual void* GetNativeHandle() const noexcept = 0;

    // ─── Chrome ───────────────────────────────────────────────────────────────

    /// Rebuild the entire menu bar from a ShellLayout descriptor.
    virtual void ApplyShellLayout(const ShellLayout& layout) = 0;

    /// Write a message to the status bar.
    virtual void SetStatusMessage(const std::string& message) = 0;
    [[nodiscard]] virtual std::optional<std::string> ChooseProjectToOpen()
    { return std::nullopt; }
    [[nodiscard]] virtual std::optional<NewProjectPrompt> ChooseNewProject()
    { return std::nullopt; }
    [[nodiscard]] virtual UnsavedDocumentsDecision ConfirmUnsavedDocuments(
        const std::vector<std::string>&)
    { return UnsavedDocumentsDecision::Cancel; }

    // ─── Panel Docking ────────────────────────────────────────────────────────

    /// Dock a panel's native widget (void* → cast to QWidget* on Qt backend)
    /// into the given area. The window takes display ownership of the widget.
    virtual void DockPanel(void*              nativeWidget,
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
};

} // namespace SagaEditor
