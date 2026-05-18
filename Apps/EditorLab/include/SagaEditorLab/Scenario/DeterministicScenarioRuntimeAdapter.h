/// @file DeterministicScenarioRuntimeAdapter.h
/// @brief Local deterministic adapter for running EditorLab scenarios without EditorHost.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioRuntimeAdapter.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditorLab
{

/// In-memory adapter used by EditorLab until a public SagaEditor adapter exists.
class DeterministicScenarioRuntimeAdapter final : public IScenarioRuntimeAdapter
{
public:
    DeterministicScenarioRuntimeAdapter();

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

    /// Override a state path for focused controller tests.
    void SetState(std::string statePath, std::string value);

    /// Return the deterministic operation log.
    [[nodiscard]] const std::vector<std::string>& GetOperations() const noexcept;

private:
    std::unordered_map<std::string, std::string> m_state;
    std::vector<std::string> m_operations;
};

} // namespace SagaEditorLab
