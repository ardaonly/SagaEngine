/// @file ValidationResult.cpp
/// @brief ValidationResult accumulation and merge logic.

#include "SDE/Validation/ValidationResult.h"

#include <algorithm>

namespace SDE
{

bool ValidationResult::HasErrors() const noexcept
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& d) {
        return d.severity == Severity::Error || d.severity == Severity::Fatal;
    });
}

bool ValidationResult::HasWarnings() const noexcept
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& d) {
        return d.severity == Severity::Warning;
    });
}

void ValidationResult::Merge(ValidationResult other)
{
    state = SDE::Merge(state, other.state);
    diagnostics.insert(
        diagnostics.end(),
        std::make_move_iterator(other.diagnostics.begin()),
        std::make_move_iterator(other.diagnostics.end()));
}

} // namespace SDE
