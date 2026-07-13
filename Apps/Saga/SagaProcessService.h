/// @file SagaProcessService.h
/// @brief Defines the typed Product Shell process execution contract.

#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <stop_token>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Allowlisted executable identity accepted by Product Shell orchestration.
enum class SagaProcessTargetId
{
    Editor,
    Runtime,
    SagaProject,
    Forge,
    SagaScript,
};

/// Whether Product Shell waits for a bounded result or completes after handoff.
enum class SagaProcessExecutionMode
{
    WaitForCompletion,
    Detached,
};

/// Stable outcome category for one process request.
enum class SagaProcessExitClassification
{
    NotStarted,
    Detached,
    Succeeded,
    Failed,
    Crashed,
    TimedOut,
    Cancelled,
    InvalidRequest,
};

/// Typed process request constructed by Product-owned services, never by shell text.
struct SagaProductProcessRequest
{
    SagaProcessTargetId target = SagaProcessTargetId::Runtime;
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    std::map<std::string, std::string> environment;
    std::chrono::milliseconds timeout{30000};
    SagaProcessExecutionMode mode = SagaProcessExecutionMode::WaitForCompletion;
    std::stop_token stopToken; ///< Cooperative cancellation requested by the launcher task.
};

/// Captured and classified result from the Product Shell process boundary.
struct SagaProductProcessResult
{
    bool started = false;
    bool timedOut = false;
    bool cancelled = false;
    int exitCode = -1;
    std::chrono::milliseconds duration{0};
    SagaProcessExitClassification classification =
        SagaProcessExitClassification::NotStarted;
    std::string standardOutput;
    std::string standardError;
    std::string error;
};

/// Product-owned process service with one Qt process implementation.
class ISagaProcessService
{
public:
    virtual ~ISagaProcessService() = default;

    /// Validate and execute a typed request without invoking a shell.
    [[nodiscard]] virtual SagaProductProcessResult Run(
        const SagaProductProcessRequest& request) = 0;
};

/// Qt implementation shared by launcher, tool, and evidence adapters.
class SagaProcessService final : public ISagaProcessService
{
public:
    [[nodiscard]] SagaProductProcessResult Run(
        const SagaProductProcessRequest& request) override;

    /// Return whether the executable name matches the typed target identity.
    [[nodiscard]] static bool IsExecutableAllowed(
        SagaProcessTargetId target,
        const std::filesystem::path& executable) noexcept;

    /// Return whether an environment override is accepted by Product policy.
    [[nodiscard]] static bool IsEnvironmentKeyAllowed(
        const std::string& key) noexcept;
};

[[nodiscard]] const char* ToString(SagaProcessTargetId target) noexcept;
[[nodiscard]] const char* ToString(
    SagaProcessExitClassification classification) noexcept;

} // namespace SagaProduct
