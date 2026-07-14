/// @file SagaEditorLabBridge.cpp
/// @brief Dev-only bridge that mounts EditorLab panels into SagaEditor.

#include "SagaEditorLabBridge.h"

#include "SagaEditorLab/UI/ScenarioRunnerDockPanel.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include <memory>

namespace SagaDev
{

void InstallSagaEditorLabBridge(SagaEditor::EditorShell& shell)
{
    constexpr const char* scenarioRunnerPanelId =
        "saga.panel.editorlab.scenario_runner";
    shell.RegisterPanel(
        std::make_unique<SagaEditorLab::ScenarioRunnerDockPanel>(shell),
        SagaEditor::UIDockArea::Bottom);
    shell.GetMainWindow().SetPanelVisible(scenarioRunnerPanelId, true);
}

} // namespace SagaDev
