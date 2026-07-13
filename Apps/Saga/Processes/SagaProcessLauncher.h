/// @file SagaProcessLauncher.h
/// @brief Product-local process launch boundary for Saga role targets.

#pragma once

#include "Processes/SagaProcessService.h"
#include "Projects/SagaSessionModel.h"

#include <chrono>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace SagaProduct
{

/// Product request for launching a prepared role executable.
struct SagaProcessLaunchRequest
{
    SagaProductTargetKind target = SagaProductTargetKind::Editor; ///< Product role being launched.
    std::filesystem::path executablePath;                         ///< Resolved executable path.
    std::vector<std::string> arguments;                            ///< Arguments passed to the target process.
    std::filesystem::path workingDirectory;                        ///< Working directory used for process execution.
    std::chrono::milliseconds timeout{30000};                      ///< Maximum wait for a bounded process.
    SagaProcessExecutionMode mode =                                ///< Wait or detached handoff policy.
        SagaProcessExecutionMode::WaitForCompletion;
};

/// Product result for a completed launch handoff.
struct SagaProcessLaunchResult
{
    bool ok = false;                                ///< True when the launched process completed successfully.
    bool started = false;                           ///< True when process creation succeeded.
    int exitCode = -1;                              ///< Child process exit code when available.
    SagaProcessExitClassification classification =  ///< Stable child outcome category.
        SagaProcessExitClassification::NotStarted;
    std::vector<SagaProductDiagnostic> diagnostics; ///< Product-level launch diagnostics.
};

/// Abstract launch boundary used by SagaApp and focused tests.
class ISagaProcessLauncher
{
public:
    virtual ~ISagaProcessLauncher() = default;

    /// Launch the requested process and forward child output to the provided streams.
    [[nodiscard]] virtual SagaProcessLaunchResult Launch(
        const SagaProcessLaunchRequest& request,
        std::ostream& out,
        std::ostream& err) = 0;
};

/// Qt-backed implementation for local product role process launches.
class SagaProcessLauncher final : public ISagaProcessLauncher
{
public:
    /// Launch a role process and wait for deterministic completion.
    [[nodiscard]] SagaProcessLaunchResult Launch(
        const SagaProcessLaunchRequest& request,
        std::ostream& out,
        std::ostream& err) override;
};

} // namespace SagaProduct
