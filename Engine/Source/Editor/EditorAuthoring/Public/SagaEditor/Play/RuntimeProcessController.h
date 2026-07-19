// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Play/PlayPreparationPipeline.h"
#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace SagaEditor
{
class EditorDocumentWorkspace;

enum class RuntimePlayState { Idle, Preparing, Starting, Running, Stopping, Completed, Failed };

struct RuntimeProcessLaunchRequest
{
    std::string playSessionId;
    std::filesystem::path runtimeExecutable;
    bool headless = false;
    bool smokeInteractions = false;
    std::chrono::milliseconds startupTimeout{30000};
    std::chrono::milliseconds runtimeTimeout{300000};
    std::chrono::milliseconds gracefulStopTimeout{5000};
    std::chrono::milliseconds sigtermGrace{1000};
    std::chrono::milliseconds sigkillGrace{1000};
    std::function<void(std::string_view)> progress;
};

struct RuntimeProcessSnapshot
{
    RuntimePlayState state = RuntimePlayState::Idle;
    std::string playSessionId;
    std::string diagnosticCode;
    std::string message;
    std::filesystem::path resultPath;
    std::filesystem::path startupReadyPath;
    int exitCode = -1;
    ToolProcessShutdownClassification shutdownClassification =
        ToolProcessShutdownClassification::None;
    std::optional<SagaRuntime::Play::RuntimeResultV1> runtimeResult;
};

/// Owns one separate SagaRuntime child process. Preparation and process I/O run
/// off the UI thread; Stop first publishes the session stop marker and only
/// then falls back to bounded process-tree termination.
class RuntimeProcessController
{
public:
    RuntimeProcessController() = default;
    ~RuntimeProcessController();
    RuntimeProcessController(const RuntimeProcessController&) = delete;
    RuntimeProcessController& operator=(const RuntimeProcessController&) = delete;

    [[nodiscard]] bool Start(EditorDocumentWorkspace& workspace,
                             RuntimeProcessLaunchRequest request);
    void Stop();
    void Shutdown();
    [[nodiscard]] RuntimeProcessSnapshot Snapshot() const;

private:
    void Run(PlaySourceSnapshot sourceSnapshot, RuntimeProcessLaunchRequest request);
    void SetState(RuntimePlayState state, std::string code = {}, std::string message = {});

    mutable std::mutex mutex_;
    RuntimeProcessSnapshot snapshot_;
    std::thread worker_;
    std::atomic_bool stopRequested_{false};
    std::filesystem::path stopPath_;
};
} // namespace SagaEditor
