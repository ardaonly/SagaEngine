/// @file IExtensionContext.h
/// @brief Context passed to extensions on load — grants access to editor services.

#pragma once

#include <memory>
#include <string>

namespace SagaEditor
{

class IPanel;
class CommandDescriptor;
class EditorTheme;
class EditorHost;

/// Panel dock area enumeration (independent of Qt).
enum class DockArea
{
    Left = 0,   ///< Left side of editor.
    Right = 1,  ///< Right side of editor.
    Top = 2,    ///< Top of editor.
    Bottom = 3, ///< Bottom of editor.
};

// ─── Extension Context ────────────────────────────────────────────────────────

/// The controlled surface extensions may call during OnLoad / OnUnload.
/// Extensions must not retain a pointer to this context beyond those calls.
class IExtensionContext
{
public:
    virtual ~IExtensionContext() = default;

    // ─── Panel Registration ───────────────────────────────────────────────────

    /// Add a panel to the editor shell. The shell takes ownership.
    virtual void RegisterPanel(std::unique_ptr<IPanel> panel,
                               DockArea area = DockArea::Right) = 0;

    /// Remove a previously registered panel by PanelId.
    virtual void UnregisterPanel(const std::string& panelId) = 0;

    // ─── Command Registration ─────────────────────────────────────────────────

    /// Register a command. Unregistered automatically on UnregisterPanel or OnUnload.
    virtual void RegisterCommand(const CommandDescriptor& descriptor) = 0;

    /// Remove a command registered by this extension.
    virtual void UnregisterCommand(const std::string& commandId) = 0;

    // ─── Theme Registration ───────────────────────────────────────────────────

    /// Register an additional editor theme.
    virtual void RegisterTheme(const EditorTheme& theme) = 0;

    // ─── Host Access ──────────────────────────────────────────────────────────

    /// Full access to editor host services (selection, undo, shortcuts, etc.).
    [[nodiscard]] virtual EditorHost& GetHost() = 0;
};

} // namespace SagaEditor
