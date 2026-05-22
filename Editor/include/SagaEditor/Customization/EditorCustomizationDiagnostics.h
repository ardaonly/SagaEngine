/// @file EditorCustomizationDiagnostics.h
/// @brief Structured diagnostics for safe editor customization operations.

#pragma once

#include <string>
#include <vector>

namespace SagaEditor
{

/// Severity for safe customization diagnostics.
enum class EditorCustomizationDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    Blocker
};

/// Stable diagnostic emitted by customization capability, controller, and store code.
struct EditorCustomizationDiagnostic
{
    EditorCustomizationDiagnosticSeverity severity =
        EditorCustomizationDiagnosticSeverity::Info; ///< Diagnostic severity.
    std::string code;                                ///< Stable machine-readable code.
    std::string message;                             ///< Human-readable explanation.
    std::string targetId;                            ///< Panel/action/workspace id where applicable.
    std::string path;                                ///< File path or logical path where applicable.
};

/// Return true when diagnostics contain an error or blocker.
[[nodiscard]] bool HasErrorCustomizationDiagnostic(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics);

/// Return true when diagnostics contain a blocker.
[[nodiscard]] bool HasBlockingCustomizationDiagnostic(
    const std::vector<EditorCustomizationDiagnostic>& diagnostics);

/// Append a customization diagnostic to a vector.
void AddCustomizationDiagnostic(
    std::vector<EditorCustomizationDiagnostic>& diagnostics,
    EditorCustomizationDiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string targetId = {},
    std::string path = {});

} // namespace SagaEditor
