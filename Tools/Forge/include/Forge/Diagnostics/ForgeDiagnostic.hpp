/// @file ForgeDiagnostic.hpp
/// @brief Forge-local structured diagnostic contract.

#pragma once

#include <map>
#include <string>

namespace Forge::Diagnostics
{

/// Minimal Forge diagnostic severity vocabulary.
enum class ForgeDiagnosticSeverity
{
    Info,
    Warning,
    Error,
    BuildBlocking,
};

/// Structured diagnostic owned by Forge tool orchestration.
struct ForgeDiagnostic
{
    ForgeDiagnosticSeverity severity = ForgeDiagnosticSeverity::Info;
    std::string code;
    std::string title;
    std::string message;
    std::string stepId;
    std::string reference;
    std::map<std::string, std::string> metadata;
};

} // namespace Forge::Diagnostics
