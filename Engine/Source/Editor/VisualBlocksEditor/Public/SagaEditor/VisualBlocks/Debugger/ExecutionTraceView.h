/// @file ExecutionTraceView.h
/// @brief Backend-neutral visual scripting execution trace view contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor::VisualBlocks
{

/// Dockable debugger trace surface; Qt widget ownership stays private.
class ExecutionTraceView final : public SagaEditor::IPanel
{
public:
    ExecutionTraceView();
    ~ExecutionTraceView() override;

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
