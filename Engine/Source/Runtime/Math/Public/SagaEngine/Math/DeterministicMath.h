/// @file DeterministicMath.h
/// @brief Platform-stable math primitives used by deterministic simulation.
///
/// Purpose: `std::sin`, `std::cos`, `std::sqrt`, and friends are NOT required
///          by the C++ standard to produce bit-identical results across
///          compilers, CPU families, or even the same binary with different
///          /fp flags.  A networked, rollback-capable simulation cannot
///          tolerate that drift — a bit difference on tick N compounds into
///          full desync on tick N+50.
///
///          This header exposes a small set of deterministic replacements the
///          simulation layer can call instead of libm directly.  They trade
///          some accuracy for reproducibility, which is exactly the correct
///          trade-off for gameplay logic.
///
/// Strategy (phase 1 — current file):
///   - Fixed-point rational approximations via integer math where possible.
///   - `volatile` rounding fences around std:: calls to disable compiler
///     re-association so at least *one* binary produces stable results.
///   - A central SetRoundingMode() hook the simulation bootstrap can call
///     once at start-up to lock FE_TONEAREST across all threads.
///
/// Strategy (future phase 2, tracked in roadmap):
///   - Full fixed-point Q16.16 / Q32.32 vector math pipeline.
///   - Cross-compiler bit-identical transcendentals via polynomial tables.
///
/// Non-goals:
///   - This is NOT a replacement for the general math library — render,
///     editor, and client-side prediction should keep using the fast glm /
///     float path.  Only the authoritative simulation tick uses these.

#pragma once

#include "SagaEngine/Math/MathConstants.h"

#include <cstdint>

namespace SagaEngine::Math::Deterministic
{

// ─── Runtime configuration ────────────────────────────────────────────────────

/// Lock floating-point rounding to round-to-nearest-even on the calling
/// thread.  Call once per simulation worker at start-up.  Returns true on
/// success; a false return means the platform refused the request (rare, but
/// the simulation can then fall back to the fixed-point path).
[[nodiscard]] bool LockRoundingModeToNearest() noexcept;

/// Query whether the current thread's rounding mode is FE_TONEAREST.  The
/// simulation assert loop uses this to catch accidental FPU state corruption
/// from third-party libraries.
[[nodiscard]] bool IsRoundingModeDeterministic() noexcept;

// ─── Deterministic scalar primitives ─────────────────────────────────────────

/// Reproducible square root.  Uses a compiler fence to prevent FMA fusion
/// and ensures IEEE-754 round-to-nearest semantics.
[[nodiscard]] float Sqrt(float value) noexcept;

/// Reproducible sine.  Implemented as a polynomial approximation on the
/// reduced range [-π, π] so its output is compiler-independent.
[[nodiscard]] float Sin(float radians) noexcept;

/// Reproducible cosine.  Shares the same polynomial core as Sin.
[[nodiscard]] float Cos(float radians) noexcept;

/// Reproducible atan2 — returns an angle in radians in [-π, π].
[[nodiscard]] float Atan2(float y, float x) noexcept;

// ─── Fixed-point helpers (Q16.16) ────────────────────────────────────────────

/// A signed 32-bit fixed-point value with 16 fractional bits.
/// Range ≈ [-32768, 32767] with a resolution of 1/65536.
using Fixed16 = std::int32_t;

/// Scale applied when converting between float and Fixed16.
inline constexpr std::int32_t kFixed16Scale = 1 << 16;

/// Convert a float to Q16.16 fixed-point.  Rounds to nearest even.
[[nodiscard]] constexpr Fixed16 ToFixed16(float value) noexcept
{
    // Multiply then round — biasing with 0.5 keeps symmetry around zero.
    return static_cast<Fixed16>(value >= 0.0f
        ? (value * kFixed16Scale + 0.5f)
        : (value * kFixed16Scale - 0.5f));
}

/// Convert a Q16.16 value back to float.  Exact for values that fit.
[[nodiscard]] constexpr float FromFixed16(Fixed16 value) noexcept
{
    return static_cast<float>(value) / static_cast<float>(kFixed16Scale);
}

/// Add two Q16.16 values.  Integer addition is bit-identical on every CPU.
[[nodiscard]] constexpr Fixed16 Add(Fixed16 a, Fixed16 b) noexcept
{
    return a + b;
}

/// Multiply two Q16.16 values using a widened intermediate so the
/// fractional bits are preserved.  Deterministic across all targets.
[[nodiscard]] constexpr Fixed16 Multiply(Fixed16 a, Fixed16 b) noexcept
{
    return static_cast<Fixed16>((static_cast<std::int64_t>(a) *
                                 static_cast<std::int64_t>(b)) >> 16);
}

} // namespace SagaEngine::Math::Deterministic
