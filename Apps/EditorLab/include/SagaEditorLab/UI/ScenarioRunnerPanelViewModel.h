/// @file ScenarioRunnerPanelViewModel.h
/// @brief Qt-free view-model for the EditorLab Scenario Runner panel.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioDefinition.h"
#include "SagaEditorLab/Scenario/ScenarioResult.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditorLab
{

class IScenarioRuntimeAdapter;

/// Scenario row shown by the runner panel.
struct ScenarioRunnerScenarioItem
{
    std::string id;              ///< Stable scenario id.
    std::string name;            ///< Display name.
    std::uint32_t stepCount = 0; ///< Number of scenario steps.
};

/// Runtime adapter row shown by the runner panel.
struct ScenarioRunnerRuntimeModeItem
{
    std::string id;   ///< Stable runtime mode id.
    std::string name; ///< Display name.
};

/// Diagnostic row projected from ScenarioResult.
struct ScenarioRunnerDiagnosticRow
{
    std::string severity; ///< Display severity.
    std::string message;  ///< Diagnostic message.
    std::int32_t stepIndex = -1;
    std::string stepLabel;
};

/// Snapshot row projected from ScenarioResult.
struct ScenarioRunnerSnapshotRow
{
    std::string label; ///< Snapshot label.
    std::string sha256; ///< Optional hash, empty when hashing is disabled.
    std::int32_t stepIndex = -1;
};

/// Compact result summary shown above diagnostics.
struct ScenarioRunnerResultSummary
{
    bool hasResult = false;
    std::string verdict;
    std::uint32_t stepsExecuted = 0;
    std::uint32_t stepsTotal = 0;
    std::size_t warningCount = 0;
    std::size_t errorCount = 0;
    std::size_t diagnosticCount = 0;
    std::size_t snapshotCount = 0;
};

/// Owns scenario selection and execution state for the UI.
class ScenarioRunnerPanelViewModel
{
public:
    ScenarioRunnerPanelViewModel();
    explicit ScenarioRunnerPanelViewModel(IScenarioRuntimeAdapter& runtimeAdapter);
    explicit ScenarioRunnerPanelViewModel(std::vector<ScenarioDefinition> scenarios);
    ScenarioRunnerPanelViewModel(std::vector<ScenarioDefinition> scenarios,
                                 IScenarioRuntimeAdapter& runtimeAdapter);

    /// Return all selectable scenario rows.
    [[nodiscard]] std::vector<ScenarioRunnerScenarioItem> GetScenarioItems() const;

    /// Select a scenario by id.
    [[nodiscard]] bool SelectScenario(const std::string& scenarioId);

    /// Return the selected scenario, or null when no scenario is selected.
    [[nodiscard]] const ScenarioDefinition* GetSelectedScenario() const noexcept;

    /// Return all runtime modes available for this runner instance.
    [[nodiscard]] std::vector<ScenarioRunnerRuntimeModeItem> GetRuntimeModeItems() const;

    /// Return the selected runtime mode id.
    [[nodiscard]] const std::string& GetSelectedRuntimeModeId() const noexcept;

    /// Select a runtime mode by id.
    [[nodiscard]] bool SelectRuntimeMode(const std::string& modeId);

    /// Run the selected scenario through the selected runtime mode.
    [[nodiscard]] bool RunSelectedScenario();

    /// Run a scenario directly and store its result for display.
    void RunScenario(const ScenarioDefinition& scenario);

    [[nodiscard]] ScenarioRunnerResultSummary GetResultSummary() const;
    [[nodiscard]] std::vector<ScenarioRunnerDiagnosticRow> GetDiagnosticRows() const;
    [[nodiscard]] std::vector<ScenarioRunnerSnapshotRow> GetSnapshotRows() const;
    [[nodiscard]] const ScenarioResult* GetLastResult() const noexcept;

private:
    std::vector<ScenarioDefinition> m_scenarios;
    IScenarioRuntimeAdapter* m_runtimeAdapter = nullptr;
    std::string m_selectedRuntimeModeId = "deterministic";
    std::size_t m_selectedIndex = 0;
    bool m_hasSelection = false;
    ScenarioResult m_lastResult;
    bool m_hasResult = false;
};

} // namespace SagaEditorLab
