/// @file DiagnosticCode.hpp
/// @brief Shared stable diagnostic code value.

#pragma once

#include <string>

namespace SagaShared::Diagnostics
{

/// Stable machine-readable diagnostic code, for example "Build.Step.MissingOutput".
struct DiagnosticCode
{
    std::string value; ///< Stable code used by tools, CI filters, reports, and tests.
};

} // namespace SagaShared::Diagnostics
