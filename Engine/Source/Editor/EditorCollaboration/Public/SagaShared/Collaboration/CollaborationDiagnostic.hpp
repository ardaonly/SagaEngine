/// @file CollaborationDiagnostic.hpp
/// @brief Shared diagnostic payload for collaboration services and UI adapters.

#pragma once

#include <cstdint>
#include <string>

namespace SagaShared::Collaboration
{

/// Severity vocabulary for collaboration diagnostics.
enum class CollaborationDiagnosticSeverity : std::uint8_t
{
    Info,
    Warning,
    Error,
};

/// Diagnostic payload that can cross product, editor, runtime, and server boundaries.
struct CollaborationDiagnostic
{
    CollaborationDiagnosticSeverity severity = CollaborationDiagnosticSeverity::Info; ///< Diagnostic severity.
    std::string code;        ///< Stable machine-readable diagnostic code.
    std::string message;     ///< Human-readable diagnostic message.
    std::string resourceId;  ///< Optional affected resource identifier.
};

} // namespace SagaShared::Collaboration
