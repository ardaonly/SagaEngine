/// @file ProductionDashboardPanel.h
/// @brief First production readiness dashboard for SagaEditor.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

class EditorHost;

class ProductionDashboardPanel final : public IPanel
{
public:
    explicit ProductionDashboardPanel(EditorHost& host);
    ~ProductionDashboardPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

    void Refresh();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
