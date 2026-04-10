/// @file MathConstants.h
/// @brief Centralised compile-time math constants for the SagaEngine math module.
///
/// Purpose: Replace ad-hoc literals (pi, epsilon, deg/rad conversions) that
///          would otherwise be duplicated across translation units with a
///          single source of truth.  Duplicating these values risks silent
///          precision drift between modules and breaks deterministic
///          reproducibility across platforms, which matters for networked
///          simulation.
///
/// Design:
///   - All constants are `inline constexpr` so they have internal linkage per
///     translation unit and are usable in `constexpr` contexts.
///   - Both `float` and `double` variants are provided; float is the engine
///     default, double is offered for high-precision tooling (editor, tests).
///   - No GLM dependency — these constants are the foundation other math
///     headers build on, so they must stay GLM-free.

#pragma once

namespace SagaEngine::Math
{

// ─── Pi family (float) ────────────────────────────────────────────────────────

/// π as a single-precision float.  Use this in place of any `3.14159f` literal.
inline constexpr float kPi        = 3.14159265358979323846f;
/// 2π — full turn in radians.
inline constexpr float kTwoPi     = 6.28318530717958647692f;
/// π/2 — quarter turn in radians.
inline constexpr float kHalfPi    = 1.57079632679489661923f;
/// π/4 — eighth turn in radians.
inline constexpr float kQuarterPi = 0.78539816339744830962f;
/// 1/π — reciprocal used to convert radians to rotations.
inline constexpr float kInvPi     = 0.31830988618379067154f;

// ─── Pi family (double) ───────────────────────────────────────────────────────

/// Double-precision π for tools that need extra precision (editor, offline).
inline constexpr double kPiD     = 3.14159265358979323846;
inline constexpr double kTwoPiD  = 6.28318530717958647692;
inline constexpr double kHalfPiD = 1.57079632679489661923;

// ─── Epsilons ─────────────────────────────────────────────────────────────────

/// Generic small-value epsilon for float comparisons in gameplay code.
/// Chosen to be larger than typical accumulated FMA rounding noise but small
/// enough to reject any meaningful difference (≈ 1/1,000,000 unit).
inline constexpr float kEpsilon       = 1.0e-6f;

/// Tighter epsilon used by normalisation guards — below this, a vector is
/// treated as the zero vector to avoid dividing by near-zero length.
inline constexpr float kEpsilonNormSq = 1.0e-12f;

/// Dot-product threshold above which two quaternions are considered
/// co-directional and can skip a full SLERP in favour of NLERP.
inline constexpr float kSlerpLinearThreshold = 0.9995f;

// ─── Unit conversions ─────────────────────────────────────────────────────────

/// Multiply by this to convert an angle in degrees to radians.
inline constexpr float kDegToRad = kPi / 180.0f;

/// Multiply by this to convert an angle in radians to degrees.
inline constexpr float kRadToDeg = 180.0f / kPi;

// ─── Convenience helpers (thin so they stay in this header) ──────────────────

/// Convert degrees to radians at compile time.
[[nodiscard]] inline constexpr float DegreesToRadians(float degrees) noexcept
{
    return degrees * kDegToRad;
}

/// Convert radians to degrees at compile time.
[[nodiscard]] inline constexpr float RadiansToDegrees(float radians) noexcept
{
    return radians * kRadToDeg;
}

} // namespace SagaEngine::Math
