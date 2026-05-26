/// @file ProjectBrowserPanel.h
/// @brief Backend-neutral project browser panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

/// Dockable project browser surface; Qt widget ownership stays private.
class ProjectBrowserPanel final : public IPanel
{
public:
    ProjectBrowserPanel();
    ~ProjectBrowserPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor
