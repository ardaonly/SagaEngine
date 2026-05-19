/// @file DiagnosticPayload.hpp
/// @brief Shared structured diagnostic payload.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticCode.hpp"
#include "SagaShared/Diagnostics/DiagnosticLocation.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <map>
#include <string>

namespace SagaShared::Diagnostics
{

/// Data-only diagnostic payload suitable for reports, tooling, and UI adapters.
struct DiagnosticPayload
{
    DiagnosticSeverity severity = DiagnosticSeverity::Info; ///< Diagnostic severity.
    DiagnosticCategory category = DiagnosticCategory::Project; ///< Top-level category family.
    DiagnosticSource source = DiagnosticSource::Product; ///< Producer family.
    DiagnosticCode code;        ///< Stable machine-readable diagnostic code.
    std::string title;          ///< Short human-readable title, empty when unavailable.
    std::string message;        ///< Human-readable diagnostic message.
    std::string technicalDetails; ///< Optional technical detail for advanced consumers.
    DiagnosticLocation location; ///< Optional source/resource/build/artifact location.
    std::string suggestedAction; ///< Optional remediation hint.
    std::map<std::string, std::string> metadata; ///< Optional serialization-safe metadata.
};

} // namespace SagaShared::Diagnostics
