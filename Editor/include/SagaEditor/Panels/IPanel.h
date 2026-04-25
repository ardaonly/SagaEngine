/// @file IPanel.h
/// @brief Base interface for every dockable editor panel — framework-free.

#pragma once

#include <string>

namespace SagaEditor
{

// ─── Panel Identity ───────────────────────────────────────────────────────────

/// Stable dot-namespaced type identifier (e.g. "saga.panel.hierarchy").
/// Package panels must use a reverse-DNS prefix to avoid collisions.
using PanelId = std::string;

// ─── Panel Interface ──────────────────────────────────────────────────────────

/// Abstract base for every dockable panel. Pure C++ — no Qt dependency.
/// The concrete Qt backend provides QtPanel : public QWidget, public IPanel.
/// Derived types must be registerable by PanelId so the layout serializer can
/// reconstruct the workspace from a saved JSON file.
class IPanel
{
public:
    virtual ~IPanel() = default;

    // ─── Identity ─────────────────────────────────────────────────────────────

    /// Return the stable type identifier used by the layout serializer.
    [[nodiscard]] virtual PanelId     GetPanelId() const = 0;

    /// Return the human-readable title shown in the dock tab.
    [[nodiscard]] virtual std::string GetTitle()   const = 0;

    /// Return the underlying native widget pointer (e.g. QWidget* on Qt).
    /// Only the UI backend should cast this; all other code stays framework-free.
    [[nodiscard]] virtual void* GetNativeWidget() const noexcept = 0;

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    /// Called once after the panel widget is constructed and the host is ready.
    virtual void OnInit()     {}

    /// Called once before the panel is destroyed.
    virtual void OnShutdown() {}

    // ─── Focus ────────────────────────────────────────────────────────────────

    /// Called when the dock widget hosting this panel gains keyboard focus.
    virtual void OnFocusGained() {}

    /// Called when the dock widget hosting this panel loses keyboard focus.
    virtual void OnFocusLost()   {}
};

} // namespace SagaEditor
