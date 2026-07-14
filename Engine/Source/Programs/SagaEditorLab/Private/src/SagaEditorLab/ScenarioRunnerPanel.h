/// @file ScenarioRunnerPanel.h
/// @brief Qt widget wrapper for the EditorLab Scenario Runner panel.

#pragma once

#include "SagaEditorLab/UI/ScenarioRunnerPanelViewModel.h"

#include <memory>

class QWidget;

namespace SagaEditorLab
{

class IScenarioRuntimeAdapter;

/// Thin Qt panel that renders ScenarioRunnerPanelViewModel state.
class ScenarioRunnerPanel final
{
public:
    ScenarioRunnerPanel();
    explicit ScenarioRunnerPanel(IScenarioRuntimeAdapter& runtimeAdapter);
    ~ScenarioRunnerPanel();

    [[nodiscard]] QWidget* Widget() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditorLab
