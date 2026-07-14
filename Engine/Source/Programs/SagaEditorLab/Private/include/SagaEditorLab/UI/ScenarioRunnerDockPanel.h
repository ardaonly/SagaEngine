/// @file ScenarioRunnerDockPanel.h
/// @brief Editor dock panel wrapper for the EditorLab Scenario Runner.

#pragma once

#include "SagaEditor/Panels/IPanel.h"

#include <memory>
#include <string>

namespace SagaEditor
{
class EditorShell;
}

namespace SagaEditorLab
{

/// Mounts the Scenario Runner as a dockable editor panel with a shell adapter.
class ScenarioRunnerDockPanel final : public SagaEditor::IPanel
{
public:
    explicit ScenarioRunnerDockPanel(SagaEditor::EditorShell& shell);
    ~ScenarioRunnerDockPanel() override;

    [[nodiscard]] SagaEditor::PanelId GetPanelId() const override;
    [[nodiscard]] std::string GetTitle() const override;
    [[nodiscard]] void* GetNativeWidget() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditorLab
