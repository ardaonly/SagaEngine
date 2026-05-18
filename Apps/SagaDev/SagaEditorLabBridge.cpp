/// @file SagaEditorLabBridge.cpp
/// @brief Dev-only bridge that mounts EditorLab panels into Saga editor mode.

#include "SagaEditorLabBridge.h"

#include "SagaEditorLab/UI/ScenarioRunnerDockPanel.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"
#include "SagaEditorModule.h"

#include <memory>

namespace SagaDev
{

void InstallSagaEditorLabBridge()
{
    static bool installed = false;
    if (installed)
    {
        return;
    }

    SagaProduct::RegisterEditorModePanelProvider(
        [](SagaEditor::EditorShell& shell)
        {
            constexpr const char* scenarioRunnerPanelId =
                "saga.panel.editorlab.scenario_runner";
            shell.RegisterPanel(
                std::make_unique<SagaEditorLab::ScenarioRunnerDockPanel>(shell),
                SagaEditor::UIDockArea::Bottom);
            shell.GetMainWindow().SetPanelVisible(scenarioRunnerPanelId, true);
        });
    installed = true;
}

} // namespace SagaDev
