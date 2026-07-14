/// @file ScriptDiagnosticPayload.hpp
/// @brief Shared SagaScript diagnostic payload contract.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticPayload.hpp"

#include <map>
#include <string>

namespace SagaShared::Scripting
{

/// Script-specific diagnostic wrapper with stable script and binding context.
struct ScriptDiagnosticPayload
{
    std::string scriptId;                         ///< Stable script identifier, empty when unavailable.
    std::string bindingId;                        ///< Binding identifier, empty when diagnostic is source-wide.
    std::string artifactId;                       ///< Artifact identifier, empty before artifact emission.
    Diagnostics::DiagnosticPayload diagnostic;    ///< Neutral diagnostic payload consumed by reports and UI.
    std::map<std::string, std::string> metadata;  ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
