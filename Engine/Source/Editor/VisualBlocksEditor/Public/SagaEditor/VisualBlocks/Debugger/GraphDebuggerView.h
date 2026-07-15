/// @file GraphDebuggerView.h
/// @brief Backend-neutral visual scripting graph debugger view contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor::VisualBlocks
{

/// Dockable graph debugger surface; Qt widget ownership stays private.
class GraphDebuggerView final : public SagaEditor::IPanel
{
public:
    GraphDebuggerView();
    ~GraphDebuggerView() override;

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
