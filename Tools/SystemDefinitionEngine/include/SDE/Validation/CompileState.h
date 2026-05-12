/// @file CompileState.h
/// @brief Fine-grained pipeline state propagated through the compilation pass.

#pragma once

#include <string>

namespace SDE
{

// ─── CompileState ─────────────────────────────────────────────────────────────

/// Fine-grained state of the compilation pipeline.
///
/// Enumerators are ordered by severity: each subsumes the ones before it.
/// Use Merge() to combine states from parallel pipeline stages.
enum class CompileState
{
    Clean,            ///< No issues; output is fully usable.
    WithWarnings,     ///< Warnings only; output is still usable.
    UnresolvedRefs,   ///< At least one dangling reference; output is incomplete.
    ValidationFailed, ///< Business rules failed; output is incorrect.
    Aborted,          ///< Pipeline halted early (Fatal diagnostic or abortOnFirstError).
    IOError,          ///< File could not be read or written.
};

/// Return the more severe of two states.
[[nodiscard]] CompileState Merge(CompileState a, CompileState b) noexcept;

/// Return true when the state indicates a usable output (Clean or WithWarnings).
[[nodiscard]] bool IsUsable(CompileState s) noexcept;

/// Return a stable human-readable name for the state.
[[nodiscard]] std::string StateName(CompileState s);

// ─── Relational operators ─────────────────────────────────────────────────────
// enum class has no built-in ordering; these enable severity comparisons.

[[nodiscard]] constexpr bool operator< (CompileState a, CompileState b) noexcept
{ return static_cast<int>(a) <  static_cast<int>(b); }
[[nodiscard]] constexpr bool operator<=(CompileState a, CompileState b) noexcept
{ return static_cast<int>(a) <= static_cast<int>(b); }
[[nodiscard]] constexpr bool operator> (CompileState a, CompileState b) noexcept
{ return static_cast<int>(a) >  static_cast<int>(b); }
[[nodiscard]] constexpr bool operator>=(CompileState a, CompileState b) noexcept
{ return static_cast<int>(a) >= static_cast<int>(b); }

} // namespace SDE
