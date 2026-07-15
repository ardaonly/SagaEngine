/// @file NodePalette.h
/// @brief Backend-neutral visual scripting node palette panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor::VisualBlocks
{

/// Dockable node palette surface; Qt widget ownership stays private.
class NodePalette final : public SagaEditor::IPanel
{
public:
    NodePalette();
    ~NodePalette() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor::VisualBlocks
