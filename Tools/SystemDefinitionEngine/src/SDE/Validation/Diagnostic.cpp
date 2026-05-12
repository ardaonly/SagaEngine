/// @file Diagnostic.cpp
/// @brief Factory implementations for the Diagnostic value type.

#include "SDE/Validation/Diagnostic.h"

namespace SDE
{

Diagnostic Diagnostic::MakeInfo(SourceLocation loc, std::string code, std::string message)
{
    return {Severity::Info, std::move(loc), std::move(code), std::move(message), {}};
}

Diagnostic Diagnostic::MakeWarning(SourceLocation loc, std::string code, std::string message)
{
    return {Severity::Warning, std::move(loc), std::move(code), std::move(message), {}};
}

Diagnostic Diagnostic::MakeError(SourceLocation loc, std::string code, std::string message)
{
    return {Severity::Error, std::move(loc), std::move(code), std::move(message), {}};
}

Diagnostic Diagnostic::MakeFatal(SourceLocation loc, std::string code, std::string message)
{
    return {Severity::Fatal, std::move(loc), std::move(code), std::move(message), {}};
}

} // namespace SDE
