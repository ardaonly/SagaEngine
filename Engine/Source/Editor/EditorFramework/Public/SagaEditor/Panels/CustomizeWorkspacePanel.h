/// @file CustomizeWorkspacePanel.h
/// @brief Safe Customize Workspace panel backed by editor customization session.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

class EditorHost;

/// Thin dockable surface for safe user workspace customization overlays.
class CustomizeWorkspacePanel final : public IPanel
{
public:
    explicit CustomizeWorkspacePanel(EditorHost& host);
    ~CustomizeWorkspacePanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

    /// Refresh rows and status from the host customization session.
    void Refresh();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
