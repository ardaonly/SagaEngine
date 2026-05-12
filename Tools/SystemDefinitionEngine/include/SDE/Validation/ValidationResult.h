/// @file ValidationResult.h
/// @brief Aggregate outcome of a validation pass: state + diagnostic list.

#pragma once

#include "SDE/Validation/CompileState.h"
#include "SDE/Validation/Diagnostic.h"

#include <vector>

namespace SDE
{

// ─── ValidationResult ─────────────────────────────────────────────────────────

/// Accumulates diagnostics and tracks the worst CompileState seen across pipeline stages.
struct ValidationResult
{
    CompileState            state = CompileState::Clean;
    std::vector<Diagnostic> diagnostics;

    /// Return true when at least one Error or Fatal diagnostic is present.
    [[nodiscard]] bool HasErrors() const noexcept;

    /// Return true when at least one Warning diagnostic is present.
    [[nodiscard]] bool HasWarnings() const noexcept;

    /// Append another result's diagnostics and take the worse state.
    void Merge(ValidationResult other);
};

} // namespace SDE
