/// @file InspectorPanel.h
/// @brief Component inspector panel — edits properties of selected entities.
///
/// Qt-free header. All widget internals live in the Impl (InspectorPanel.cpp).
/// The panel pulls registered editor factories from `ComponentEditorRegistry`
/// and forwards property edits through `InspectorBinder`, so the Qt widget
/// surface never has to know the per-component schema.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>

namespace SagaEditor
{

class ComponentEditorRegistry;
class InspectorBinder;
class SelectionManager;

// ─── Inspector Panel ──────────────────────────────────────────────────────────

/// Shows editable property widgets for whatever is currently selected.
/// Connects to `SelectionManager` (for the active entity), to
/// `ComponentEditorRegistry` (to find the per-component editor
/// factory), and to `InspectorBinder` (to forward edits into the undo
/// pipeline). All three are non-owning pointers; the editor host owns
/// the lifetime.
class InspectorPanel : public IPanel
{
public:
    InspectorPanel();
    ~InspectorPanel() override;

    // ─── IPanel ───────────────────────────────────────────────────────────────
    [[nodiscard]] PanelId     GetPanelId()      const override;
    [[nodiscard]] std::string GetTitle()        const override;
    [[nodiscard]] void*       GetNativeWidget() const noexcept override;

    void OnInit()     override;
    void OnShutdown() override;

    // ─── Bindings ─────────────────────────────────────────────────────────────

    /// Source of the active selection. The panel listens for selection
    /// changes and rebuilds the editor list. May be nullptr to detach.
    void SetSelectionManager(SelectionManager* mgr) noexcept;

    /// Registry of per-component editor factories. May be nullptr;
    /// without a registry the panel falls back to the
    /// "Nothing selected." placeholder.
    void SetComponentEditorRegistry(ComponentEditorRegistry* registry) noexcept;

    /// Edit forwarder. May be nullptr; without a binder, edits made
    /// inside child editors do not propagate beyond the panel.
    void SetInspectorBinder(InspectorBinder* binder) noexcept;

    // ─── Rebuild ──────────────────────────────────────────────────────────────

    /// Rebuild the editor list from the supplied component-type list.
    /// Called by the selection bridge after resolving which components
    /// the active entity carries; the panel queries the registry for
    /// each type and inserts the produced editor in iteration order.
    void RefreshFromTypes(const std::vector<std::type_index>& componentTypes);

    /// Drop every child editor and show the empty-state placeholder.
    /// Equivalent to `RefreshFromTypes({})`.
    void ClearEditors();

    /// Number of editor panels currently nested inside the inspector.
    [[nodiscard]] std::size_t EditorCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
