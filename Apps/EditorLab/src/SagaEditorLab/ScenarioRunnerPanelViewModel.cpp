/// @file ScenarioRunnerPanelViewModel.cpp
/// @brief View-model logic for the EditorLab Scenario Runner panel.

#include "SagaEditorLab/UI/ScenarioRunnerPanelViewModel.h"
#include "SagaEditorLab/Scenario/BuiltinScenarioDefinitions.h"
#include "SagaEditorLab/Scenario/DeterministicScenarioRuntimeAdapter.h"
#include "SagaEditorLab/Scenario/ScenarioRunner.h"

#include <algorithm>
#include <utility>

namespace SagaEditorLab
{
namespace
{

[[nodiscard]] std::string SeverityText(ScenarioDiagnostic::Severity severity)
{
    switch (severity)
    {
        case ScenarioDiagnostic::Severity::Info: return "Info";
        case ScenarioDiagnostic::Severity::Warning: return "Warning";
        case ScenarioDiagnostic::Severity::Error: return "Error";
        case ScenarioDiagnostic::Severity::Fatal: return "Fatal";
    }

    return "Info";
}

} // namespace

ScenarioRunnerPanelViewModel::ScenarioRunnerPanelViewModel()
    : ScenarioRunnerPanelViewModel(MakeBuiltinScenarioDefinitions())
{}

ScenarioRunnerPanelViewModel::ScenarioRunnerPanelViewModel(
    std::vector<ScenarioDefinition> scenarios)
    : m_scenarios(std::move(scenarios))
{
    m_hasSelection = !m_scenarios.empty();
}

std::vector<ScenarioRunnerScenarioItem>
ScenarioRunnerPanelViewModel::GetScenarioItems() const
{
    std::vector<ScenarioRunnerScenarioItem> items;
    items.reserve(m_scenarios.size());

    for (const ScenarioDefinition& scenario : m_scenarios)
    {
        ScenarioRunnerScenarioItem item;
        item.id = scenario.id;
        item.name = scenario.name;
        item.stepCount = static_cast<std::uint32_t>(scenario.steps.size());
        items.push_back(std::move(item));
    }

    return items;
}

bool ScenarioRunnerPanelViewModel::SelectScenario(const std::string& scenarioId)
{
    const auto found = std::find_if(
        m_scenarios.begin(),
        m_scenarios.end(),
        [&scenarioId](const ScenarioDefinition& scenario)
        {
            return scenario.id == scenarioId;
        });

    if (found == m_scenarios.end())
    {
        return false;
    }

    m_selectedIndex = static_cast<std::size_t>(
        std::distance(m_scenarios.begin(), found));
    m_hasSelection = true;
    return true;
}

const ScenarioDefinition*
ScenarioRunnerPanelViewModel::GetSelectedScenario() const noexcept
{
    if (!m_hasSelection || m_selectedIndex >= m_scenarios.size())
    {
        return nullptr;
    }

    return &m_scenarios[m_selectedIndex];
}

bool ScenarioRunnerPanelViewModel::RunSelectedScenario()
{
    const ScenarioDefinition* selected = GetSelectedScenario();
    if (!selected)
    {
        return false;
    }

    RunScenario(*selected);
    return true;
}

void ScenarioRunnerPanelViewModel::RunScenario(const ScenarioDefinition& scenario)
{
    DeterministicScenarioRuntimeAdapter adapter;
    m_lastResult = ScenarioRunner{}.Run(scenario, adapter);
    m_hasResult = true;
}

ScenarioRunnerResultSummary
ScenarioRunnerPanelViewModel::GetResultSummary() const
{
    ScenarioRunnerResultSummary summary;
    summary.hasResult = m_hasResult;
    if (!m_hasResult)
    {
        return summary;
    }

    summary.verdict = ScenarioVerdictId(m_lastResult.verdict);
    summary.stepsExecuted = m_lastResult.stepsExecuted;
    summary.stepsTotal = m_lastResult.stepsTotal;
    summary.warningCount = m_lastResult.WarningCount();
    summary.errorCount = m_lastResult.ErrorCount();
    summary.diagnosticCount = m_lastResult.diagnostics.size();
    summary.snapshotCount = m_lastResult.snapshots.size();
    return summary;
}

std::vector<ScenarioRunnerDiagnosticRow>
ScenarioRunnerPanelViewModel::GetDiagnosticRows() const
{
    std::vector<ScenarioRunnerDiagnosticRow> rows;
    if (!m_hasResult)
    {
        return rows;
    }

    rows.reserve(m_lastResult.diagnostics.size());
    for (const ScenarioDiagnostic& diagnostic : m_lastResult.diagnostics)
    {
        ScenarioRunnerDiagnosticRow row;
        row.severity = SeverityText(diagnostic.severity);
        row.message = diagnostic.message;
        row.stepIndex = diagnostic.stepIndex;
        row.stepLabel = diagnostic.stepLabel;
        rows.push_back(std::move(row));
    }

    return rows;
}

std::vector<ScenarioRunnerSnapshotRow>
ScenarioRunnerPanelViewModel::GetSnapshotRows() const
{
    std::vector<ScenarioRunnerSnapshotRow> rows;
    if (!m_hasResult)
    {
        return rows;
    }

    rows.reserve(m_lastResult.snapshots.size());
    for (const ScenarioSnapshotRef& snapshot : m_lastResult.snapshots)
    {
        ScenarioRunnerSnapshotRow row;
        row.label = snapshot.label;
        row.sha256 = snapshot.sha256;
        row.stepIndex = snapshot.stepIndex;
        rows.push_back(std::move(row));
    }

    return rows;
}

const ScenarioResult*
ScenarioRunnerPanelViewModel::GetLastResult() const noexcept
{
    return m_hasResult ? &m_lastResult : nullptr;
}

} // namespace SagaEditorLab
