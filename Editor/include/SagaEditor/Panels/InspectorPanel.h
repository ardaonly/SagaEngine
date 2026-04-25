/// @file InspectorPanel.h
/// @brief Component inspector panel — edits properties of selected entities.
///
/// Qt-free header. All widget internals live in the Impl (InspectorPanel.cpp).

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <memory>
#include <string>

namespace SagaEditor
{

class SelectionManager;

// ─── Inspector Panel ──────────────────────────────────────────────────────────

/// Shows editable property widgets for whatever is currently selected.
/// Connects to SelectionManager and rebuilds on every selection change.
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
    void SetSelectionManager(SelectionManager* mgr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
