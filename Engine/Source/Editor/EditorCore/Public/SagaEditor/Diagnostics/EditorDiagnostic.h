/// @file EditorDiagnostic.h
/// @brief Diagnostic records emitted by editor subsystems and displayed by panels.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Diagnostic Severity ─────────────────────────────────────────────────────

/// User-facing severity used by the Problems panel and publish gates.
enum class EditorDiagnosticSeverity : uint8_t
{
    Info,
    Warning,
    Error,
};

// ─── Diagnostic Identity ─────────────────────────────────────────────────────

using EditorDiagnosticId = uint64_t;
inline constexpr EditorDiagnosticId kInvalidEditorDiagnosticId = 0;

// ─── Diagnostic Location ─────────────────────────────────────────────────────

/// Optional resource position attached to a diagnostic.
struct EditorDiagnosticLocation
{
    std::string resource; ///< Project-relative asset, scene, graph, or document id.
    int line = 0;         ///< One-based source line when available; zero means unknown.
    int column = 0;       ///< One-based source column when available; zero means unknown.
};

// ─── Diagnostic Record ───────────────────────────────────────────────────────

/// Immutable diagnostic payload stored by the editor diagnostics service.
struct EditorDiagnostic
{
    EditorDiagnosticId id = kInvalidEditorDiagnosticId; ///< Stable id assigned by the service.
    EditorDiagnosticSeverity severity = EditorDiagnosticSeverity::Info;
    std::string source = "editor"; ///< Producing subsystem, e.g. "asset-import" or "script-compiler".
    std::string code;              ///< Stable machine-readable diagnostic code.
    std::string message;           ///< User-facing problem description.
    EditorDiagnosticLocation location;
    bool publishBlocking = false;  ///< True when builds/publishing should be blocked.

    [[nodiscard]] bool operator==(const EditorDiagnostic& other) const noexcept
    {
        return id == other.id &&
               severity == other.severity &&
               source == other.source &&
               code == other.code &&
               message == other.message &&
               location.resource == other.location.resource &&
               location.line == other.location.line &&
               location.column == other.location.column &&
               publishBlocking == other.publishBlocking;
    }
};

} // namespace SagaEditor
