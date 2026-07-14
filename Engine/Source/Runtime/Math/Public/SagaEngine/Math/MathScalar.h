/// @file MathScalar.h
/// @brief Scalar math helpers shared by all engine subsystems.
///
/// Purpose: Collect the tiny but critical scalar utilities (clamp, lerp,
///          smoothstep, approx-equal, saturate, sign) that gameplay,
///          replication, and interpolation code otherwise reinvent locally.
///          Keeping them in one header guarantees consistent semantics —
///          e.g. Lerp contract, ApproxEqual epsilon — across modules.
///
/// Design:
///   - Header-only, constexpr where possible, noexcept throughout.
///   - Overloads for float and double; integer clamp is templated.
///   - Intentionally mirrors std::clamp / std::lerp shape so switching to
///     <algorithm> / <numeric> later would be a trivial rename.

#pragma once

#include "SagaEngine/Math/MathConstants.h"

#include <cmath>

namespace SagaEngine::Math
{

// ─── Clamp ────────────────────────────────────────────────────────────────────

/// Clamp a value to the inclusive range [lo, hi].  Templated so it works for
/// float, double, and integral types without extra overloads.
template <typename T>
[[nodiscard]] constexpr T Clamp(T value, T lo, T hi) noexcept
{
    return (value < lo) ? lo : (hi < value ? hi : value);
}

/// Clamp a value into [0, 1] — the normalised form used by lerp parameters
/// and colour channels.
[[nodiscard]] constexpr float Saturate(float value) noexcept
{
    return Clamp(value, 0.0f, 1.0f);
}

// ─── Linear interpolation ────────────────────────────────────────────────────

/// Standard linear interpolation.  `t` is NOT clamped — callers wanting
/// clamped behaviour should call Saturate on `t` first.
[[nodiscard]] constexpr float Lerp(float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

/// Double-precision lerp for editor / offline tools.
[[nodiscard]] constexpr double Lerp(double a, double b, double t) noexcept
{
    return a + (b - a) * t;
}

/// Inverse lerp: where does `value` sit in the [a, b] range as a [0, 1]
/// parameter?  Returns 0 if `a == b` to avoid division by zero.
[[nodiscard]] constexpr float InverseLerp(float a, float b, float value) noexcept
{
    return (a == b) ? 0.0f : (value - a) / (b - a);
}

// ─── Smooth interpolation ─────────────────────────────────────────────────────

/// Hermite smoothstep — S-curve on [0, 1].  Input is clamped internally.
/// Used for client-side smoothing between state snapshots.
[[nodiscard]] constexpr float SmoothStep(float t) noexcept
{
    const float x = Saturate(t);
    return x * x * (3.0f - 2.0f * x);
}

/// Quintic smootherstep — zero first and second derivative at endpoints.
/// Slightly more expensive but eliminates visible acceleration jumps.
[[nodiscard]] constexpr float SmootherStep(float t) noexcept
{
    const float x = Saturate(t);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// ─── Approximate equality ────────────────────────────────────────────────────

/// Compare two floats with an absolute tolerance.  Default tolerance is the
/// engine-wide `kEpsilon` — override for tighter / looser contexts.
[[nodiscard]] inline bool ApproxEqual(float a,
                                      float b,
                                      float epsilon = kEpsilon) noexcept
{
    return std::fabs(a - b) <= epsilon;
}

/// Compare two floats using a relative tolerance, so the epsilon scales with
/// the magnitude of the inputs.  Use this when values may span orders of
/// magnitude (physics velocities, world coordinates far from origin).
[[nodiscard]] inline bool ApproxEqualRelative(float a,
                                              float b,
                                              float relEpsilon = kEpsilon) noexcept
{
    const float diff  = std::fabs(a - b);
    const float scale = std::fabs(a) > std::fabs(b) ? std::fabs(a) : std::fabs(b);
    return diff <= relEpsilon * (scale > 1.0f ? scale : 1.0f);
}

/// Test whether a float is effectively zero with the engine's default epsilon.
[[nodiscard]] inline bool IsNearZero(float value,
                                     float epsilon = kEpsilon) noexcept
{
    return std::fabs(value) <= epsilon;
}

// ─── Sign / step / repeat ────────────────────────────────────────────────────

/// Return -1, 0, or +1 according to the sign of `value`.
[[nodiscard]] constexpr float Sign(float value) noexcept
{
    return (value > 0.0f) - (value < 0.0f);
}

/// Wrap `value` into [0, length) — used for angle normalisation and tiled
/// world coordinates.  `length` must be > 0.
[[nodiscard]] inline float Repeat(float value, float length) noexcept
{
    return value - std::floor(value / length) * length;
}

/// Normalise an angle in radians to [-π, π].  Useful for interpolating
/// Euler yaw values received from the network without wrap artefacts.
[[nodiscard]] inline float WrapAnglePi(float radians) noexcept
{
    return Repeat(radians + kPi, kTwoPi) - kPi;
}

// ─── Angular lerp ────────────────────────────────────────────────────────────

/// Shortest-arc linear interpolation between two angles in radians.
/// Handles the ±π wrap-around that naive Lerp gets wrong.
[[nodiscard]] inline float LerpAngle(float a, float b, float t) noexcept
{
    const float delta = WrapAnglePi(b - a);
    return a + delta * t;
}

} // namespace SagaEngine::Math
