/// @file ScenarioRuntimeAdapter.cpp
/// @brief Common adapter result builders for EditorLab scenario execution.

#include "SagaEditorLab/Scenario/ScenarioRuntimeAdapter.h"

#include <utility>

namespace SagaEditorLab
{

ScenarioAdapterResult ScenarioAdapterResult::Success()
{
    return {};
}

ScenarioAdapterResult ScenarioAdapterResult::Failure(std::string message)
{
    ScenarioAdapterResult result;
    result.succeeded = false;
    result.message = std::move(message);
    return result;
}

ScenarioStateReadResult ScenarioStateReadResult::Value(std::string value)
{
    ScenarioStateReadResult result;
    result.value = std::move(value);
    return result;
}

ScenarioStateReadResult ScenarioStateReadResult::Missing(std::string message)
{
    ScenarioStateReadResult result;
    result.hasValue = false;
    result.message = std::move(message);
    return result;
}

ScenarioStateReadResult ScenarioStateReadResult::Failure(std::string message)
{
    ScenarioStateReadResult result;
    result.succeeded = false;
    result.message = std::move(message);
    return result;
}

ScenarioSnapshotCaptureResult ScenarioSnapshotCaptureResult::Captured(
    ScenarioSnapshotRef snapshot)
{
    ScenarioSnapshotCaptureResult result;
    result.snapshot = std::move(snapshot);
    return result;
}

ScenarioSnapshotCaptureResult ScenarioSnapshotCaptureResult::Failure(std::string message)
{
    ScenarioSnapshotCaptureResult result;
    result.succeeded = false;
    result.message = std::move(message);
    return result;
}

} // namespace SagaEditorLab
