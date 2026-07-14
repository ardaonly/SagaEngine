/// @file ScenarioResult.cpp
/// @brief Verdict helpers and diagnostic aggregation for EditorLab scenarios.

#include "SagaEditorLab/Scenario/ScenarioResult.h"

#include <algorithm>
#include <utility>

namespace SagaEditorLab
{

const char* ScenarioVerdictId(ScenarioVerdict v) noexcept
{
    switch (v)
    {
        case ScenarioVerdict::Pass:             return "pass";
        case ScenarioVerdict::PassWithWarnings: return "pass_with_warnings";
        case ScenarioVerdict::Fail:             return "fail";
        case ScenarioVerdict::Error:            return "error";
        case ScenarioVerdict::Skipped:          return "skipped";
    }
    return "unknown";
}

bool ScenarioResult::Ok() const noexcept
{
    return verdict == ScenarioVerdict::Pass ||
           verdict == ScenarioVerdict::PassWithWarnings;
}

bool ScenarioResult::HasErrors() const noexcept
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const ScenarioDiagnostic& diagnostic)
        {
            return diagnostic.severity == ScenarioDiagnostic::Severity::Error ||
                   diagnostic.severity == ScenarioDiagnostic::Severity::Fatal;
        });
}

std::size_t ScenarioResult::WarningCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        diagnostics.begin(),
        diagnostics.end(),
        [](const ScenarioDiagnostic& diagnostic)
        {
            return diagnostic.severity == ScenarioDiagnostic::Severity::Warning;
        }));
}

std::size_t ScenarioResult::ErrorCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        diagnostics.begin(),
        diagnostics.end(),
        [](const ScenarioDiagnostic& diagnostic)
        {
            return diagnostic.severity == ScenarioDiagnostic::Severity::Error ||
                   diagnostic.severity == ScenarioDiagnostic::Severity::Fatal;
        }));
}

void ScenarioResult::AddDiagnostic(ScenarioDiagnostic diag)
{
    switch (diag.severity)
    {
        case ScenarioDiagnostic::Severity::Info:
            break;
        case ScenarioDiagnostic::Severity::Warning:
            if (verdict == ScenarioVerdict::Pass)
            {
                verdict = ScenarioVerdict::PassWithWarnings;
            }
            break;
        case ScenarioDiagnostic::Severity::Error:
            if (verdict != ScenarioVerdict::Error)
            {
                verdict = ScenarioVerdict::Fail;
            }
            break;
        case ScenarioDiagnostic::Severity::Fatal:
            verdict = ScenarioVerdict::Error;
            break;
    }

    diagnostics.push_back(std::move(diag));
}

} // namespace SagaEditorLab
