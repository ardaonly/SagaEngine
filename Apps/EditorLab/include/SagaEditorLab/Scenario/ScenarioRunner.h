/// @file ScenarioRunner.h
/// @brief Deterministic headless runner for EditorLab scenarios.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioDefinition.h"
#include "SagaEditorLab/Scenario/ScenarioResult.h"
#include "SagaEditorLab/Scenario/ScenarioRuntimeAdapter.h"

namespace SagaEditorLab
{

/// Execution controls for ScenarioRunner.
struct ScenarioRunnerOptions
{
    bool continueAfterFailure = false; ///< Continue after step failure and accumulate diagnostics.
};

/// Runs pure scenario data through a caller-supplied runtime adapter.
class ScenarioRunner
{
public:
    [[nodiscard]] ScenarioResult Run(const ScenarioDefinition& scenario,
                                     IScenarioRuntimeAdapter& adapter,
                                     const ScenarioRunnerOptions& options = {}) const;
};

} // namespace SagaEditorLab
