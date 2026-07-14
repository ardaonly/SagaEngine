/// @file BreakpointPanel.h
/// @brief Backend-neutral visual scripting breakpoint panel contract.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>

namespace SagaEditor::VisualScripting
{

/// Dockable debugger breakpoint surface; Qt widget ownership stays private.
class BreakpointPanel final : public SagaEditor::IPanel
{
public:
    BreakpointPanel();
    ~BreakpointPanel() override;

    [[nodiscard]] PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

    void OnInit() override;
    void OnShutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor::VisualScripting
