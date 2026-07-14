/// @file HealthSeverity.hpp
/// @brief Defines severity levels for diagnostics health rule results.

#pragma once

#include <cstdint>

namespace SagaEngine::Diagnostics
{

/// Severity assigned to a health rule result.
enum class HealthSeverity : std::uint8_t
{
    Info,
    Warning,
    Error,
    Critical,
};

/// Convert a health severity to its stable report string.
[[nodiscard]] const char* ToString(HealthSeverity severity) noexcept;

} // namespace SagaEngine::Diagnostics
