/// @file CollaborationPanel.h
/// @brief Backend-neutral collaboration panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

/// Dockable collaboration surface; Qt widget ownership stays private.
class CollaborationPanel final : public IPanel
{
public:
    CollaborationPanel();
    ~CollaborationPanel() override;

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
