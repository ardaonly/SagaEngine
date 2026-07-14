/// @file EditorShellScenarioRuntimeAdapter.cpp
/// @brief EditorShell-backed scenario adapter for EditorLab panel visibility checks.

#include "SagaEditorLab/Scenario/EditorShellScenarioRuntimeAdapter.h"

#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Shell/EditorShell.h"
#include "SagaEditor/UI/IUIMainWindow.h"

namespace SagaEditorLab
{

EditorShellScenarioRuntimeAdapter::EditorShellScenarioRuntimeAdapter(
    SagaEditor::EditorShell& shell) noexcept
    : m_shell(shell)
    , m_hostAdapter(shell.GetHost())
{}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::DispatchAction(
    const std::string& commandId,
    const std::string& argument)
{
    return m_hostAdapter.DispatchAction(commandId, argument);
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::OpenPanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }
    if (m_shell.FindPanel(panelId) == nullptr)
    {
        return ScenarioAdapterResult::Failure("panel is not registered: " + panelId);
    }

    m_shell.GetMainWindow().SetPanelVisible(panelId, true);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::ClosePanel(
    const std::string& panelId)
{
    if (panelId.empty())
    {
        return ScenarioAdapterResult::Failure("panel id is empty");
    }
    if (m_shell.FindPanel(panelId) == nullptr)
    {
        return ScenarioAdapterResult::Failure("panel is not registered: " + panelId);
    }

    m_shell.GetMainWindow().SetPanelVisible(panelId, false);
    return ScenarioAdapterResult::Success();
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::LoadProject(
    const std::string& path)
{
    return m_hostAdapter.LoadProject(path);
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::SetSelection(
    const std::string& entityIds)
{
    return m_hostAdapter.SetSelection(entityIds);
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::Undo()
{
    return m_hostAdapter.Undo();
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::Redo()
{
    return m_hostAdapter.Redo();
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::Save()
{
    return m_hostAdapter.Save();
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::RegisterMockExtension(
    const std::string& extensionId,
    const std::string& manifestJson)
{
    return m_hostAdapter.RegisterMockExtension(extensionId, manifestJson);
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::AdvanceTicks(
    std::uint32_t tickCount)
{
    return m_hostAdapter.AdvanceTicks(tickCount);
}

ScenarioAdapterResult EditorShellScenarioRuntimeAdapter::SleepMilliseconds(
    std::uint32_t milliseconds)
{
    return m_hostAdapter.SleepMilliseconds(milliseconds);
}

ScenarioSnapshotCaptureResult EditorShellScenarioRuntimeAdapter::CaptureSnapshot(
    const std::string& label)
{
    return m_hostAdapter.CaptureSnapshot(label);
}

ScenarioStateReadResult EditorShellScenarioRuntimeAdapter::ReadState(
    const std::string& statePath)
{
    return m_hostAdapter.ReadState(statePath);
}

} // namespace SagaEditorLab
