/// @file EditorHostScenarioRuntimeAdapter.h
/// @brief EditorHost-backed scenario adapter for EditorLab service-level checks.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioRuntimeAdapter.h"

namespace SagaEditor
{
class EditorHost;
}

namespace SagaEditorLab
{

/// Routes EditorLab scenario operations through SagaEditor public services.
class EditorHostScenarioRuntimeAdapter final : public IScenarioRuntimeAdapter
{
public:
    explicit EditorHostScenarioRuntimeAdapter(SagaEditor::EditorHost& host) noexcept;

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
    SagaEditor::EditorHost& m_host; ///< Non-owning; caller controls host lifetime.
};

} // namespace SagaEditorLab
