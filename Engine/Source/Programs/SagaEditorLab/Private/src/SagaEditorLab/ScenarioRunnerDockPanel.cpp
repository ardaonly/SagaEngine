/// @file ScenarioRunnerDockPanel.cpp
/// @brief Editor dock panel wrapper for the EditorLab Scenario Runner.

#include "SagaEditorLab/UI/ScenarioRunnerDockPanel.h"

#include "SagaEditorLab/Scenario/EditorShellScenarioRuntimeAdapter.h"

#include "ScenarioRunnerPanel.h"

namespace SagaEditorLab
{
namespace
{

constexpr const char* kScenarioRunnerPanelId =
    "saga.panel.editorlab.scenario_runner";
constexpr const char* kScenarioRunnerPanelTitle = "EditorLab Scenarios";

} // namespace

struct ScenarioRunnerDockPanel::Impl
{
    explicit Impl(SagaEditor::EditorShell& editorShell) noexcept
        : adapter(editorShell)
    {}

    void EnsurePanel() const
    {
        if (!panel)
        {
            panel = std::make_unique<ScenarioRunnerPanel>(adapter);
        }
    }

    mutable EditorShellScenarioRuntimeAdapter adapter;
    mutable std::unique_ptr<ScenarioRunnerPanel> panel;
};

ScenarioRunnerDockPanel::ScenarioRunnerDockPanel(SagaEditor::EditorShell& shell)
    : m_impl(std::make_unique<Impl>(shell))
{}

ScenarioRunnerDockPanel::~ScenarioRunnerDockPanel() = default;

SagaEditor::PanelId ScenarioRunnerDockPanel::GetPanelId() const
{
    return kScenarioRunnerPanelId;
}

std::string ScenarioRunnerDockPanel::GetTitle() const
{
    return kScenarioRunnerPanelTitle;
}

void* ScenarioRunnerDockPanel::GetNativeWidget() const noexcept
{
    m_impl->EnsurePanel();
    return m_impl->panel->Widget();
}

} // namespace SagaEditorLab
