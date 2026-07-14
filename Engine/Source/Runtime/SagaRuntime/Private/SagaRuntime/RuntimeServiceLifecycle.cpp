/// @file RuntimeServiceLifecycle.cpp
/// @brief Runtime service lifecycle value type implementation.

#include "SagaRuntime/RuntimeServiceLifecycle.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace SagaRuntime
{
namespace
{

[[nodiscard]] bool HasVisibleCharacter(const std::string& value) noexcept
{
    return std::any_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    });
}

[[nodiscard]] bool HasErrorDiagnostic(
    const std::vector<RuntimeServiceDiagnostic>& diagnostics) noexcept
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const RuntimeServiceDiagnostic& diagnostic) {
            return diagnostic.severity == RuntimeServiceDiagnosticSeverity::Error;
        });
}

} // namespace

bool RuntimeServiceDescriptor::IsValid() const noexcept
{
    return HasVisibleCharacter(serviceId) && HasVisibleCharacter(displayName);
}

bool RuntimeServiceLifecycleResult::Succeeded() const noexcept
{
    return state != RuntimeServiceState::Failed &&
           !HasErrorDiagnostic(diagnostics);
}

bool RuntimeServiceLifecycle::CanTransition(RuntimeServiceState from,
                                            RuntimeServiceState to) noexcept
{
    if (from == to)
    {
        return to == RuntimeServiceState::Stopped ||
               to == RuntimeServiceState::Failed;
    }

    switch (from)
    {
        case RuntimeServiceState::Registered:
            return to == RuntimeServiceState::Started ||
                   to == RuntimeServiceState::Failed;
        case RuntimeServiceState::Started:
            return to == RuntimeServiceState::Stopped ||
                   to == RuntimeServiceState::Failed;
        case RuntimeServiceState::Stopped:
        case RuntimeServiceState::Failed:
            return false;
    }

    return false;
}

RuntimeServiceLifecycleResult RuntimeServiceLifecycle::Transition(
    RuntimeServiceDescriptor descriptor,
    RuntimeServiceState from,
    RuntimeServiceState to,
    std::vector<RuntimeServiceDiagnostic> diagnostics)
{
    RuntimeServiceLifecycleResult result;
    result.descriptor = std::move(descriptor);
    result.state = to;
    result.diagnostics = std::move(diagnostics);

    if (!result.descriptor.IsValid())
    {
        result.state = RuntimeServiceState::Failed;
        result.diagnostics.push_back(Diagnostic(
            RuntimeServiceDiagnosticSeverity::Error,
            result.descriptor.serviceId,
            "Runtime service descriptor must provide serviceId and displayName."));
        return result;
    }

    if (!CanTransition(from, to))
    {
        result.state = RuntimeServiceState::Failed;
        result.diagnostics.push_back(Diagnostic(
            RuntimeServiceDiagnosticSeverity::Error,
            result.descriptor.serviceId,
            "Runtime service lifecycle transition is not allowed."));
    }

    return result;
}

RuntimeServiceDiagnostic RuntimeServiceLifecycle::Diagnostic(
    RuntimeServiceDiagnosticSeverity severity,
    std::string serviceId,
    std::string message)
{
    RuntimeServiceDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.serviceId = std::move(serviceId);
    diagnostic.message = std::move(message);
    return diagnostic;
}

} // namespace SagaRuntime
