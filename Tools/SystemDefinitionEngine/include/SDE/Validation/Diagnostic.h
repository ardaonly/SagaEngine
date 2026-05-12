/// @file Diagnostic.h
/// @brief Compiler-style diagnostic attached to a source location.

#pragma once

#include <string>

namespace SDE
{

// ─── Severity ─────────────────────────────────────────────────────────────────

/// Severity level for a diagnostic message.
enum class Severity { Info, Warning, Error, Fatal };

// ─── SourceLocation ───────────────────────────────────────────────────────────

/// File, line, and column that a diagnostic points at.
struct SourceLocation
{
    std::string file;       ///< Absolute or repo-relative path; empty when unknown.
    int         line   = 0; ///< 1-based; 0 = unknown.
    int         column = 0; ///< 1-based; 0 = unknown.
};

// ─── Diagnostic ───────────────────────────────────────────────────────────────

/// Single compiler-style diagnostic: severity, location, machine code, human message, fix hint.
struct Diagnostic
{
    Severity       severity;
    SourceLocation location;
    std::string    code;        ///< Machine-readable tag, e.g. "SDE0042".
    std::string    message;
    std::string    suggestion;  ///< Optional fix hint; empty when unavailable.

    [[nodiscard]] static Diagnostic MakeInfo   (SourceLocation loc,
                                                std::string    code,
                                                std::string    message);
    [[nodiscard]] static Diagnostic MakeWarning(SourceLocation loc,
                                                std::string    code,
                                                std::string    message);
    [[nodiscard]] static Diagnostic MakeError  (SourceLocation loc,
                                                std::string    code,
                                                std::string    message);
    [[nodiscard]] static Diagnostic MakeFatal  (SourceLocation loc,
                                                std::string    code,
                                                std::string    message);
};

} // namespace SDE
