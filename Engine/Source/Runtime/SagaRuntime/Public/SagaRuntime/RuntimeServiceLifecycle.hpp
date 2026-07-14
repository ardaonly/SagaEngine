/// @file RuntimeServiceLifecycle.hpp
/// @brief Runtime-owned service lifecycle value types.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Runtime service lifecycle state.
enum class RuntimeServiceState
{
    Registered = 0,
    Started,
    Stopped,
    Failed,
};

/// Runtime service lifecycle diagnostic severity.
enum class RuntimeServiceDiagnosticSeverity
{
    Info = 0,
    Warning,
    Error,
};

/// Stable Runtime service identity.
struct RuntimeServiceDescriptor
{
    std::string serviceId;              ///< Stable service id used by Runtime diagnostics.
    std::string displayName;            ///< Human-readable service name for app-owned presentation.
    std::optional<std::string> category; ///< Optional broad service category.

    /// Return true when the descriptor has a usable stable id and display name.
    [[nodiscard]] bool IsValid() const noexcept;
};

/// Runtime service lifecycle diagnostic.
struct RuntimeServiceDiagnostic
{
    RuntimeServiceDiagnosticSeverity severity =
        RuntimeServiceDiagnosticSeverity::Error;
    std::string serviceId;
    std::string message;
};

/// Runtime service lifecycle operation result.
struct RuntimeServiceLifecycleResult
{
    RuntimeServiceDescriptor descriptor;
    RuntimeServiceState state = RuntimeServiceState::Registered;
    std::vector<RuntimeServiceDiagnostic> diagnostics;

    /// Return true when the requested lifecycle state was reached without an error diagnostic.
    [[nodiscard]] bool Succeeded() const noexcept;
};

/// Pure Runtime service lifecycle helpers without app/client implementation ownership.
class RuntimeServiceLifecycle
{
public:
    /// Return true when a state transition is valid for the generic Runtime lifecycle.
    [[nodiscard]] static bool CanTransition(RuntimeServiceState from,
                                            RuntimeServiceState to) noexcept;

    /// Build a lifecycle result for a requested transition.
    [[nodiscard]] static RuntimeServiceLifecycleResult Transition(
        RuntimeServiceDescriptor descriptor,
        RuntimeServiceState from,
        RuntimeServiceState to,
        std::vector<RuntimeServiceDiagnostic> diagnostics = {});

    /// Build a diagnostic for a Runtime service.
    [[nodiscard]] static RuntimeServiceDiagnostic Diagnostic(
        RuntimeServiceDiagnosticSeverity severity,
        std::string serviceId,
        std::string message);
};

} // namespace SagaRuntime
