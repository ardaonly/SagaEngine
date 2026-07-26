// SPDX-License-Identifier: MPL-2.0
/// @file HierarchyPanel.h
/// @brief Scene entity hierarchy panel — tree view of all world entities.
///
/// Qt-free header. All widget internals live in the Impl (HierarchyPanel.cpp).

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <memory>
#include <string>

namespace SagaEditor
{

class SelectionManager;
class EditorDocumentWorkspace;

// ─── Hierarchy Panel ──────────────────────────────────────────────────────────

/// Displays the ECS entity tree for the open scene or world zone.
/// Clicking an entity updates SelectionManager; the Inspector reacts via callback.
class HierarchyPanel : public IPanel
{
public:
    HierarchyPanel();
    ~HierarchyPanel() override;

    // ─── IPanel ───────────────────────────────────────────────────────────────
    [[nodiscard]] PanelId     GetPanelId()       const override;
    [[nodiscard]] std::string GetTitle()         const override;
    [[nodiscard]] QWidget*    Widget()           const noexcept override;

    void OnInit()     override;
    void OnShutdown() override;

    // ─── Bindings ─────────────────────────────────────────────────────────────
    void SetSelectionManager(SelectionManager* mgr);
    void SetDocumentWorkspace(EditorDocumentWorkspace* workspace);

private:
    struct Impl;                    ///< Qt widgets live here — header stays clean.
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
