/// @file GraphViewportPanel.h
/// @brief Backend-neutral graph viewport panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor
{

/// Dockable graph viewport surface; Qt widget ownership stays private.
class GraphViewportPanel final : public IPanel
{
public:
    GraphViewportPanel();
    ~GraphViewportPanel() override;

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
