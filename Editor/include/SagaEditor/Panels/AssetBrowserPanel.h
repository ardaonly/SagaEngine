/// @file AssetBrowserPanel.h
/// @brief Asset browser panel — directory tree + asset grid.
///
/// Qt-free header. All widget internals live in the Impl (AssetBrowserPanel.cpp).

#pragma once

#include "SagaEditor/Panels/IPanel.h"
#include <memory>
#include <string>

namespace SagaEditor
{

// ─── Asset Browser Panel ──────────────────────────────────────────────────────

/// Left pane: workspace directory tree. Right pane: asset thumbnail grid.
/// Selecting a directory filters the grid. Double-clicking an asset opens it.
class AssetBrowserPanel : public IPanel
{
public:
    AssetBrowserPanel();
    ~AssetBrowserPanel() override;

    // ─── IPanel ───────────────────────────────────────────────────────────────
    [[nodiscard]] PanelId     GetPanelId()      const override;
    [[nodiscard]] std::string GetTitle()        const override;
    [[nodiscard]] void*       GetNativeWidget() const noexcept override;

    void OnInit()     override;
    void OnShutdown() override;

    // ─── Configuration ────────────────────────────────────────────────────────
    /// Point the browser at the project workspace root.
    void SetWorkspaceRoot(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
