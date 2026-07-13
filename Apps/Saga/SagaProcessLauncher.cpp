/// @file SagaProcessLauncher.cpp
/// @brief Qt-backed process launch boundary for Saga role targets.

#include "SagaProcessLauncher.h"

#include <ostream>
#include <string>
#include <utility>

namespace SagaProduct
{
namespace
{

[[nodiscard]] SagaProductDiagnostic MakeLaunchDiagnostic(
    SagaProductTargetKind target,
    const char* diagnosticId,
    std::string message,
    std::filesystem::path path)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = target;
    diagnostic.phase = SagaProductDiagnosticPhase::StartupHandoff;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] std::optional<SagaProcessTargetId> ToProcessTarget(
    SagaProductTargetKind target)
{
    switch (target)
    {
        case SagaProductTargetKind::Editor:
            return SagaProcessTargetId::Editor;
        case SagaProductTargetKind::Runtime:
            return SagaProcessTargetId::Runtime;
        case SagaProductTargetKind::Server:
            return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

SagaProcessLaunchResult SagaProcessLauncher::Launch(
    const SagaProcessLaunchRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    SagaProcessLaunchResult result;

    const auto processTarget = ToProcessTarget(request.target);
    if (!processTarget.has_value())
    {
        result.classification = SagaProcessExitClassification::InvalidRequest;
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ServerExecutionUnsupported,
            "Product dedicated-server execution is not implemented.",
            request.executablePath));
        return result;
    }

    SagaProductProcessRequest processRequest;
    processRequest.target = *processTarget;
    processRequest.executable = request.executablePath;
    processRequest.arguments = request.arguments;
    processRequest.workingDirectory = request.workingDirectory;
    processRequest.timeout = request.timeout;
    processRequest.mode = request.mode;

    SagaProcessService service;
    const SagaProductProcessResult process = service.Run(processRequest);
    result.started = process.started;
    result.exitCode = process.exitCode;
    result.classification = process.classification;
    out << process.standardOutput;
    err << process.standardError;

    if (!process.started)
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessStartFailed,
            std::string(ToString(request.target)) +
                " target process failed to start: " + process.error,
            request.executablePath));
        return result;
    }

    if (process.classification == SagaProcessExitClassification::Detached)
    {
        result.ok = true;
        return result;
    }
    if (process.classification == SagaProcessExitClassification::TimedOut)
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessExitedWithFailure,
            std::string(ToString(request.target)) + " target process timed out",
            request.executablePath));
        return result;
    }
    if (process.classification == SagaProcessExitClassification::Crashed ||
        process.classification == SagaProcessExitClassification::Failed)
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessExitedWithFailure,
            std::string(ToString(request.target)) +
                " target process exited with code " +
                std::to_string(result.exitCode),
            request.executablePath));
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
