/// @file ScenarioRuntimeAdapter.h
/// @brief Narrow runtime adapter surface used by EditorLab scenario execution.

#pragma once

#include "SagaEditorLab/Scenario/ScenarioResult.h"

#include <cstdint>
#include <string>

namespace SagaEditorLab
{

/// Result returned by adapter operations that either complete or fail.
struct ScenarioAdapterResult
{
    bool succeeded = true; ///< True when the adapter completed the requested operation.
    std::string message;   ///< Deterministic failure detail when succeeded is false.

    [[nodiscard]] static ScenarioAdapterResult Success();
    [[nodiscard]] static ScenarioAdapterResult Failure(std::string message);
};

/// Result returned when a scenario assertion reads state from the adapter.
struct ScenarioStateReadResult
{
    bool succeeded = true; ///< False when the adapter could not evaluate the path.
    bool hasValue = true;  ///< False when the path is valid but no value exists.
    std::string value;     ///< String-encoded state value.
    std::string message;   ///< Deterministic failure detail when applicable.

    [[nodiscard]] static ScenarioStateReadResult Value(std::string value);
    [[nodiscard]] static ScenarioStateReadResult Missing(std::string message);
    [[nodiscard]] static ScenarioStateReadResult Failure(std::string message);
};

/// Result returned when the adapter captures an editor state snapshot.
struct ScenarioSnapshotCaptureResult
{
    bool succeeded = true;         ///< False when snapshot capture failed.
    ScenarioSnapshotRef snapshot;  ///< Snapshot reference recorded by the runner.
    std::string message;           ///< Deterministic failure detail when applicable.

    [[nodiscard]] static ScenarioSnapshotCaptureResult Captured(ScenarioSnapshotRef snapshot);
    [[nodiscard]] static ScenarioSnapshotCaptureResult Failure(std::string message);
};

/// Adapter boundary between pure EditorLab runner logic and a concrete editor host.
class IScenarioRuntimeAdapter
{
public:
    virtual ~IScenarioRuntimeAdapter() = default;

    virtual ScenarioAdapterResult DispatchAction(const std::string& commandId,
                                                 const std::string& argument) = 0;
    virtual ScenarioAdapterResult OpenPanel(const std::string& panelId) = 0;
    virtual ScenarioAdapterResult ClosePanel(const std::string& panelId) = 0;
    virtual ScenarioAdapterResult LoadProject(const std::string& path) = 0;
    virtual ScenarioAdapterResult SetSelection(const std::string& entityIds) = 0;
    virtual ScenarioAdapterResult Undo() = 0;
    virtual ScenarioAdapterResult Redo() = 0;
    virtual ScenarioAdapterResult Save() = 0;
    virtual ScenarioAdapterResult RegisterMockExtension(const std::string& extensionId,
                                                        const std::string& manifestJson) = 0;
    virtual ScenarioAdapterResult AdvanceTicks(std::uint32_t tickCount) = 0;
    virtual ScenarioAdapterResult SleepMilliseconds(std::uint32_t milliseconds) = 0;
    virtual ScenarioSnapshotCaptureResult CaptureSnapshot(const std::string& label) = 0;
    virtual ScenarioStateReadResult ReadState(const std::string& statePath) = 0;
};

} // namespace SagaEditorLab
