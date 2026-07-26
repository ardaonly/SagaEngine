// SPDX-License-Identifier: MPL-2.0
/// @file IPanel.h
/// @brief Shared identity and lifecycle contract for dockable editor panels.

#pragma once

#include <string>

class QWidget;

namespace SagaEditor
{

// ─── Panel Identity ───────────────────────────────────────────────────────────

/// Stable dot-namespaced type identifier (e.g. "saga.panel.hierarchy").
/// Package panels must use a reverse-DNS prefix to avoid collisions.
using PanelId = std::string;

// ─── Panel Interface ──────────────────────────────────────────────────────────

/// Abstract base for every dockable Qt editor panel.
/// Concrete panels expose their widget while preserving stable identity.
/// Derived types remain registerable by PanelId for workspace composition and
/// dock restoration.
class IPanel
{
public:
    virtual ~IPanel() = default;

    // ─── Identity ─────────────────────────────────────────────────────────────

    /// Return the stable type identifier used by workspace composition and dock restoration.
    [[nodiscard]] virtual PanelId     GetPanelId() const = 0;

    /// Return the human-readable title shown in the dock tab.
    [[nodiscard]] virtual std::string GetTitle()   const = 0;

    /// Return the underlying Qt widget.
    [[nodiscard]] virtual QWidget* Widget() const noexcept = 0;

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
