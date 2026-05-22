/// @file EditorCompositionDiagnostics.h
/// @brief Structured diagnostics for editor composition artifact loading and resolution.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor
{

/// Severity used by composition artifact, overlay, and resolver diagnostics.
enum class EditorCompositionDiagnosticSeverity : uint8_t
{
    Info,
    Warning,
    Error,
    Blocker,
};

/// Machine-readable diagnostic emitted while consuming compiled composition data.
struct EditorCompositionDiagnostic
{
    EditorCompositionDiagnosticSeverity severity = EditorCompositionDiagnosticSeverity::Info;
    std::string code;
    std::string message;
    std::string documentPath;
    std::string jsonPointer;
    std::string referencedId;
};

/// Return true when any diagnostic blocks snapshot consumption.
[[nodiscard]] bool HasBlockingCompositionDiagnostic(
    const std::vector<EditorCompositionDiagnostic>& diagnostics) noexcept;

/// Return true when any diagnostic is an error or blocker.
[[nodiscard]] bool HasErrorCompositionDiagnostic(
    const std::vector<EditorCompositionDiagnostic>& diagnostics) noexcept;

} // namespace SagaEditor
