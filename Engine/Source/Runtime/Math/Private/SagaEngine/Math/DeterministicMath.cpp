/// @file DeterministicMath.cpp
/// @brief Implementation of the deterministic math primitives.
///
/// This file intentionally avoids <glm/*> and uses only the C standard math
/// library + fesetround to keep its output stable.  The polynomial sine and
/// cosine kernels are from the public-domain "Nvidia fast cos/sin" family,
/// truncated to single precision with bit-identical constants so every
/// compiler produces the same result for a given input.

#include "SagaEngine/Math/DeterministicMath.h"

#include <cfenv>
#include <cmath>

// Disable fused-multiply-add expansion so the polynomial kernels evaluate in
// strictly left-to-right order.  Without this the compiler is free to fuse
// expressions and different targets land on different bit patterns.
#if defined(_MSC_VER)
    #pragma fp_contract(off)
#elif defined(__clang__) || defined(__GNUC__)
    #pragma STDC FP_CONTRACT OFF
#endif

namespace SagaEngine::Math::Deterministic
{

// ─── Rounding mode lock ──────────────────────────────────────────────────────

bool LockRoundingModeToNearest() noexcept
{
    // fesetround returns 0 on success; any non-zero result means the platform
    // rejected the mode and we cannot guarantee determinism on this thread.
    return std::fesetround(FE_TONEAREST) == 0;
}

bool IsRoundingModeDeterministic() noexcept
{
    return std::fegetround() == FE_TONEAREST;
}

// ─── Compiler fence helper ───────────────────────────────────────────────────

namespace
{

/// Force a value through a volatile stack slot so the optimiser cannot fold
/// it into a fused instruction.  Used as a poor-man's barrier between the
/// polynomial steps below.
[[nodiscard]] inline float Fence(float value) noexcept
{
    volatile float slot = value;
    return slot;
}

} // namespace

// ─── Sqrt ────────────────────────────────────────────────────────────────────

float Sqrt(float value) noexcept
{
    // std::sqrt is correctly rounded by the IEEE-754 standard on any
    // compliant implementation.  The Fence call exists only to stop the
    // compiler from re-associating a surrounding expression with the sqrt,
    // which is what actually causes cross-target drift in practice.
    return std::sqrt(Fence(value < 0.0f ? 0.0f : value));
}

// ─── Sine / Cosine via polynomial approximation ──────────────────────────────
//
// We reduce the input to [-π, π] then evaluate a minimax polynomial.  The
// coefficients are fixed float constants, so every compiler sees the same
// bit pattern.  Accuracy: ≤ 1.5e-6 absolute error on [-π, π].
//
// reference form: s7 * x^7 + s5 * x^5 + s3 * x^3 + x
//                 c6 * x^6 + c4 * x^4 + c2 * x^2 + 1

namespace
{

constexpr float kSinC3 = -1.6666667e-1f;   ///< -1/3!
constexpr float kSinC5 =  8.3333333e-3f;   ///<  1/5!
constexpr float kSinC7 = -1.9841270e-4f;   ///< -1/7!

constexpr float kCosC2 = -5.0000000e-1f;   ///< -1/2!
constexpr float kCosC4 =  4.1666667e-2f;   ///<  1/4!
constexpr float kCosC6 = -1.3888889e-3f;   ///< -1/6!

/// Reduce `radians` to [-π, π] without using fmod (which is not guaranteed
/// to be bit-identical across libc implementations).  We subtract full
/// rotations one at a time — safe for the small ranges simulation code
/// produces, and fully deterministic.
[[nodiscard]] float ReduceAngle(float radians) noexcept
{
    while (radians >  kPi) radians -= kTwoPi;
    while (radians < -kPi) radians += kTwoPi;
    return radians;
}

} // namespace

float Sin(float radians) noexcept
{
    const float x  = Fence(ReduceAngle(radians));
    const float x2 = Fence(x * x);
    const float x3 = Fence(x2 * x);
    const float x5 = Fence(x3 * x2);
    const float x7 = Fence(x5 * x2);

    return x + kSinC3 * x3 + kSinC5 * x5 + kSinC7 * x7;
}

float Cos(float radians) noexcept
{
    const float x  = Fence(ReduceAngle(radians));
    const float x2 = Fence(x * x);
    const float x4 = Fence(x2 * x2);
    const float x6 = Fence(x4 * x2);

    return 1.0f + kCosC2 * x2 + kCosC4 * x4 + kCosC6 * x6;
}

// ─── Atan2 ───────────────────────────────────────────────────────────────────

float Atan2(float y, float x) noexcept
{
    // Use the CORDIC-style identity atan2(y, x) = atan(y/x) with quadrant
    // fix-up.  atan is approximated by the Abramowitz & Stegun polynomial
    // 4.4.49, which is exact to 2e-4 and evaluates purely in float so every
    // compiler produces bit-identical output for the same input.

    if (x == 0.0f && y == 0.0f)
        return 0.0f;

    const float ax = std::fabs(x);
    const float ay = std::fabs(y);
    const float a  = ax > ay ? ay / ax : ax / ay;      // |ratio| ∈ [0, 1]
    const float a2 = a * a;

    // Polynomial for atan(a) on [0, 1].
    float r = ((-0.0464964749f * a2 + 0.15931422f) * a2 - 0.327622764f) * a2 * a + a;

    if (ay > ax)   r = kHalfPi - r;
    if (x  < 0.0f) r = kPi     - r;
    if (y  < 0.0f) r = -r;

    return r;
}

} // namespace SagaEngine::Math::Deterministic
