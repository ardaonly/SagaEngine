/// @file PipelineDiagnostic.h
/// @brief Structured diagnostics emitted by SagaPipeline orchestration steps.

#pragma once

#include <cstdint>
#include <string>

namespace SagaPipeline
{

/// Severity for SagaPipeline orchestration diagnostics.
enum class PipelineDiagnosticSeverity : uint8_t
{
    Info,
    Warning,
    Error,
};

/// Machine-readable diagnostic produced by a pipeline step.
struct PipelineDiagnostic
{
    PipelineDiagnosticSeverity severity = PipelineDiagnosticSeverity::Info; ///< Diagnostic severity.
    std::string code;                                                        ///< Stable diagnostic code.
    std::string message;                                                     ///< Human-readable message.
};

} // namespace SagaPipeline
