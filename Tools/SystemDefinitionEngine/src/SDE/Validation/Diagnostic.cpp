/// @file Diagnostic.cpp
/// @brief Factory implementations for the Diagnostic value type.

#include "SDE/Validation/Diagnostic.h"

#include <tuple>

namespace SDE
{
namespace
{

Diagnostic MakeDiagnostic(Severity severity,
                          DiagnosticCategory category,
                          SourceLocation loc,
                          std::string code,
                          std::string message)
{
    SourceRange range{loc, loc};
    return {severity, category, std::move(loc), std::move(range),
            std::move(code), std::move(message), {}, {}};
}

} // namespace

Diagnostic Diagnostic::MakeInfo(SourceLocation loc, std::string code, std::string message)
{
    return MakeDiagnostic(Severity::Info, DiagnosticCategory::Internal,
                          std::move(loc), std::move(code), std::move(message));
}

Diagnostic Diagnostic::MakeWarning(SourceLocation loc, std::string code, std::string message)
{
    return MakeDiagnostic(Severity::Warning, DiagnosticCategory::Internal,
                          std::move(loc), std::move(code), std::move(message));
}

Diagnostic Diagnostic::MakeError(SourceLocation loc, std::string code, std::string message)
{
    return MakeDiagnostic(Severity::Error, DiagnosticCategory::Internal,
                          std::move(loc), std::move(code), std::move(message));
}

Diagnostic Diagnostic::MakeFatal(SourceLocation loc, std::string code, std::string message)
{
    return MakeDiagnostic(Severity::Fatal, DiagnosticCategory::Internal,
                          std::move(loc), std::move(code), std::move(message));
}

int SeverityRank(Severity severity) noexcept
{
    switch (severity)
    {
        case Severity::Info:    return 0;
        case Severity::Warning: return 1;
        case Severity::Error:   return 2;
        case Severity::Fatal:   return 3;
    }
    return 0;
}

const char* SeverityName(Severity severity) noexcept
{
    switch (severity)
    {
        case Severity::Info:    return "info";
        case Severity::Warning: return "warning";
        case Severity::Error:   return "error";
        case Severity::Fatal:   return "fatal";
    }
    return "info";
}

const char* DiagnosticCategoryName(DiagnosticCategory category) noexcept
{
    switch (category)
    {
        case DiagnosticCategory::IO:        return "io";
        case DiagnosticCategory::Parse:     return "parse";
        case DiagnosticCategory::Schema:    return "schema";
        case DiagnosticCategory::Type:      return "type";
        case DiagnosticCategory::Rule:      return "rule";
        case DiagnosticCategory::Reference: return "reference";
        case DiagnosticCategory::Migration: return "migration";
        case DiagnosticCategory::Import:    return "import";
        case DiagnosticCategory::Internal:  return "internal";
    }
    return "internal";
}

bool DiagnosticLess(const Diagnostic& lhs, const Diagnostic& rhs) noexcept
{
    return std::tie(lhs.location.file,
                    lhs.location.line,
                    lhs.location.column,
                    lhs.category,
                    lhs.code,
                    lhs.message) <
           std::tie(rhs.location.file,
                    rhs.location.line,
                    rhs.location.column,
                    rhs.category,
                    rhs.code,
                    rhs.message);
}

} // namespace SDE
