/// @file EditorShellScenarioRuntimeAdapter.h
/// @brief EditorShell-backed scenario adapter for EditorLab panel visibility checks.

#pragma once

#include "SagaEditorLab/Scenario/EditorHostScenarioRuntimeAdapter.h"

namespace SagaEditor
{
class EditorShell;
}

namespace SagaEditorLab
{

/// Routes panel visibility through EditorShell while reusing EditorHost services.
class EditorShellScenarioRuntimeAdapter final : public IScenarioRuntimeAdapter
{
public:
    explicit EditorShellScenarioRuntimeAdapter(SagaEditor::EditorShell& shell) noexcept;

    ScenarioAdapterResult DispatchAction(const std::string& commandId,
                                         const std::string& argument) override;
    ScenarioAdapterResult OpenPanel(const std::string& panelId) override;
    ScenarioAdapterResult ClosePanel(const std::string& panelId) override;
    ScenarioAdapterResult LoadProject(const std::string& path) override;
    ScenarioAdapterResult SetSelection(const std::string& entityIds) override;
    ScenarioAdapterResult Undo() override;
    ScenarioAdapterResult Redo() override;
    ScenarioAdapterResult Save() override;
    ScenarioAdapterResult RegisterMockExtension(const std::string& extensionId,
                                                const std::string& manifestJson) override;
    ScenarioAdapterResult AdvanceTicks(std::uint32_t tickCount) override;
    ScenarioAdapterResult SleepMilliseconds(std::uint32_t milliseconds) override;
    ScenarioSnapshotCaptureResult CaptureSnapshot(const std::string& label) override;
    ScenarioStateReadResult ReadState(const std::string& statePath) override;

private:
    SagaEditor::EditorShell& m_shell;                 ///< Non-owning shell lifetime.
    EditorHostScenarioRuntimeAdapter m_hostAdapter;   ///< Composed host adapter.
};

} // namespace SagaEditorLab
