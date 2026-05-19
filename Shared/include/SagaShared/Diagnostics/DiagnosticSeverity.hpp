/// @file DiagnosticSeverity.hpp
/// @brief Shared diagnostic severity vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Diagnostics
{

/// Stable diagnostic severity values used across reports, tools, and UI adapters.
enum class DiagnosticSeverity : std::uint8_t
{
    Trace,
    Info,
    Warning,
    Error,
    Fatal,
    BuildBlocking,
    PublishBlocking,
    Security,
};

/// Returns a stable ordering for summary aggregation.
[[nodiscard]] constexpr std::uint8_t DiagnosticSeverityRank(
    const DiagnosticSeverity severity) noexcept
{
    return static_cast<std::uint8_t>(severity);
}

} // namespace SagaShared::Diagnostics
