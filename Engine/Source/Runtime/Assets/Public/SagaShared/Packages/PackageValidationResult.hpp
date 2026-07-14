/// @file PackageValidationResult.hpp
/// @brief Shared package validation result contract.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticPayload.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"
#include "SagaShared/Packages/PackageId.hpp"

#include <vector>

namespace SagaShared::Packages
{

/// Data-only package validation result used by tools, reports, and gates.
struct PackageValidationResult
{
    PackageId packageId;
    bool valid = true;
    bool recoverable = true;
    std::vector<Diagnostics::DiagnosticPayload> diagnostics;
    Diagnostics::DiagnosticSummary summary;
};

} // namespace SagaShared::Packages
