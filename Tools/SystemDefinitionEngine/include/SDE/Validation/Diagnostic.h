/// @file Diagnostic.h
/// @brief Compiler-style diagnostic attached to a source location.

#pragma once

#include <map>
#include <string>

namespace SDE
{

// ─── Severity ─────────────────────────────────────────────────────────────────

/// Severity level for a diagnostic message.
enum class Severity { Info, Warning, Error, Fatal };

/// Compiler subsystem that produced a diagnostic. Codes remain stable public API.
enum class DiagnosticCategory
{
    IO,
    Parse,
    Schema,
    Type,
    Rule,
    Reference,
    Migration,
    Import,
    Internal,
};

// ─── SourceLocation ───────────────────────────────────────────────────────────

/// File, line, and column that a diagnostic points at.
struct SourceLocation
{
    std::string file;       ///< Absolute or repo-relative path; empty when unknown.
    int         line   = 0; ///< 1-based; 0 = unknown.
    int         column = 0; ///< 1-based; 0 = unknown.
};

/// Half-open source range. When end is unknown it may equal begin.
struct SourceRange
{
    SourceLocation begin;
    SourceLocation end;
};

// ─── Diagnostic ───────────────────────────────────────────────────────────────

/// Single compiler-style diagnostic: severity, location, machine code, human message, fix hint.
struct Diagnostic
{
    Severity                     severity;
    DiagnosticCategory           category = DiagnosticCategory::Internal;
    SourceLocation               location;
    SourceRange                  range;
    std::string                  code;        ///< Stable machine-readable tag, e.g. "SDE1004".
    std::string                  message;
    std::string                  suggestion;  ///< Optional fix hint; empty when unavailable.
    std::map<std::string, std::string> metadata;

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

[[nodiscard]] int SeverityRank(Severity severity) noexcept;
[[nodiscard]] const char* SeverityName(Severity severity) noexcept;
[[nodiscard]] const char* DiagnosticCategoryName(DiagnosticCategory category) noexcept;
[[nodiscard]] bool DiagnosticLess(const Diagnostic& lhs, const Diagnostic& rhs) noexcept;

} // namespace SDE
