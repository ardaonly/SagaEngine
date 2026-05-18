/// @file ScenarioRunner.cpp
/// @brief Headless execution loop for EditorLab scenarios.

#include "SagaEditorLab/Scenario/ScenarioRunner.h"

#include <chrono>
#include <sstream>
#include <utility>

namespace SagaEditorLab
{
namespace
{

[[nodiscard]] ScenarioDiagnostic MakeStepError(const ScenarioStep& step,
                                               std::size_t stepIndex,
                                               std::string message)
{
    ScenarioDiagnostic diagnostic;
    diagnostic.severity = ScenarioDiagnostic::Severity::Error;
    diagnostic.message = std::move(message);
    diagnostic.stepIndex = static_cast<std::int32_t>(stepIndex);
    diagnostic.stepLabel = step.label;
    return diagnostic;
}

[[nodiscard]] std::string AdapterFailureMessage(const ScenarioStep& step,
                                                const std::string& message)
{
    std::ostringstream stream;
    stream << "Step failed: " << ScenarioStepKindId(step.kind);
    if (!message.empty())
    {
        stream << ": " << message;
    }
    return stream.str();
}

[[nodiscard]] ScenarioAdapterResult ExecuteAdapterStep(
    const ScenarioStep& step,
    IScenarioRuntimeAdapter& adapter)
{
    switch (step.kind)
    {
        case ScenarioStepKind::Action:
            return adapter.DispatchAction(step.action.commandId, step.action.argument);
        case ScenarioStepKind::Wait:
            return adapter.AdvanceTicks(step.wait.tickCount);
        case ScenarioStepKind::Sleep:
            return adapter.SleepMilliseconds(step.sleep.milliseconds);
        case ScenarioStepKind::LoadProject:
            return adapter.LoadProject(step.loadProject.path);
        case ScenarioStepKind::SetSelection:
            return adapter.SetSelection(step.selection.entityIds);
        case ScenarioStepKind::Undo:
            return adapter.Undo();
        case ScenarioStepKind::Redo:
            return adapter.Redo();
        case ScenarioStepKind::Save:
            return adapter.Save();
        case ScenarioStepKind::OpenPanel:
            return adapter.OpenPanel(step.panel.panelId);
        case ScenarioStepKind::ClosePanel:
            return adapter.ClosePanel(step.panel.panelId);
        case ScenarioStepKind::RegisterMockExtension:
            return adapter.RegisterMockExtension(step.mockExtension.extensionId,
                                                 step.mockExtension.manifestJson);
        case ScenarioStepKind::Assertion:
        case ScenarioStepKind::Snapshot:
            break;
    }

    return ScenarioAdapterResult::Success();
}

[[nodiscard]] ScenarioAdapterResult ExecuteAssertionStep(
    const ScenarioStep& step,
    IScenarioRuntimeAdapter& adapter)
{
    const ScenarioStateReadResult state = adapter.ReadState(step.assertion.statePath);
    if (!state.succeeded)
    {
        return ScenarioAdapterResult::Failure(
            "State read failed for '" + step.assertion.statePath + "': " + state.message);
    }

    if (!state.hasValue)
    {
        std::string message = "Assertion failed for '" + step.assertion.statePath +
                              "': value was not available";
        if (!state.message.empty())
        {
            message += ": " + state.message;
        }
        return ScenarioAdapterResult::Failure(std::move(message));
    }

    if (state.value != step.assertion.expectedValue)
    {
        return ScenarioAdapterResult::Failure(
            "Assertion failed for '" + step.assertion.statePath + "': expected '" +
            step.assertion.expectedValue + "' but got '" + state.value + "'");
    }

    return ScenarioAdapterResult::Success();
}

} // namespace

ScenarioResult ScenarioRunner::Run(const ScenarioDefinition& scenario,
                                   IScenarioRuntimeAdapter& adapter,
                                   const ScenarioRunnerOptions& options) const
{
    const auto start = std::chrono::steady_clock::now();

    ScenarioResult result;
    result.scenarioId = scenario.id;
    result.scenarioName = scenario.name;
    result.verdict = ScenarioVerdict::Pass;
    result.stepsTotal = static_cast<std::uint32_t>(scenario.steps.size());

    for (std::size_t index = 0; index < scenario.steps.size(); ++index)
    {
        const ScenarioStep& step = scenario.steps[index];
        ++result.stepsExecuted;

        ScenarioAdapterResult stepResult = ScenarioAdapterResult::Success();

        if (step.kind == ScenarioStepKind::Assertion)
        {
            stepResult = ExecuteAssertionStep(step, adapter);
        }
        else if (step.kind == ScenarioStepKind::Snapshot)
        {
            ScenarioSnapshotCaptureResult snapshot =
                adapter.CaptureSnapshot(step.snapshot.label);
            if (snapshot.succeeded)
            {
                if (snapshot.snapshot.label.empty())
                {
                    snapshot.snapshot.label = step.snapshot.label;
                }
                if (snapshot.snapshot.stepIndex < 0)
                {
                    snapshot.snapshot.stepIndex = static_cast<std::int32_t>(index);
                }
                result.snapshots.push_back(std::move(snapshot.snapshot));
            }
            else
            {
                stepResult = ScenarioAdapterResult::Failure(
                    AdapterFailureMessage(step, snapshot.message));
            }
        }
        else
        {
            stepResult = ExecuteAdapterStep(step, adapter);
            if (!stepResult.succeeded)
            {
                stepResult.message = AdapterFailureMessage(step, stepResult.message);
            }
        }

        if (!stepResult.succeeded)
        {
            result.AddDiagnostic(MakeStepError(step, index, std::move(stepResult.message)));
            if (!options.continueAfterFailure)
            {
                break;
            }
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.wallTimeMicros = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

    return result;
}

} // namespace SagaEditorLab
