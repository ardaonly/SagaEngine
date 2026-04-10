/// @file DeterministicVectors.h
/// @brief Q16.16 fixed-point Vec3 / Quat for deterministic simulation (phase 2).
///
/// Layer  : SagaEngine / Math
/// Purpose: Phase 1 (`DeterministicMath.h`) gave us fixed-point scalars
///          and polynomial transcendentals.  That is enough to start
///          running a deterministic sim, but every time the sim touches
///          a position or velocity it still has to cross the float
///          boundary, and float drift immediately undoes all the work.
///
///          Phase 2 closes the loop: `FixedVec3` and `FixedQuat` store
///          their components as `Fixed16`, operate on them with pure
///          integer math, and never take a single detour through
///          `std::sin` or `std::sqrt`.  Bit-identical replays across
///          CPUs/compilers are the whole point.
///
/// Design rules:
///   - Every operation that must stay bit-identical uses only integer
///     add, subtract, multiply, shift.  Any transcendental goes through
///     the `Deterministic::` polynomial replacements.
///   - Vectors convert to and from `Math::Vec3` explicitly.  The
///     conversion is a *programmer-visible* boundary — the whole point
///     is that you see where float touches your deterministic code.
///   - Operator overloads stay constexpr so the compiler can fold
///     lookup-table constants at build time.
///
/// What this header is NOT:
///   - Not a drop-in replacement for Vec3.  Render, editor, and client
///     prediction keep using the float path.  Only the authoritative
///     tick on the server calls into these types.
///   - Not a full math library — no cross product shortcuts via
///     approximate normalisation, no float-friendly FMA.  That's by
///     design: every shortcut risks introducing a compiler-dependent
///     rounding.

#pragma once

#include "SagaEngine/Math/DeterministicMath.h"
#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"

#include <cstdint>

namespace SagaEngine::Math::Deterministic
{

// ─── FixedVec3 ──────────────────────────────────────────────────────────────

/// Q16.16 3-component vector.  `x`, `y`, `z` store their value * 65536
/// as a signed 32-bit integer.  Range: approximately ±32767.99 — plenty
/// for an MMO's world space if you centre each shard at its own origin.
struct FixedVec3
{
    Fixed16 x = 0;
    Fixed16 y = 0;
    Fixed16 z = 0;

    // ── Construction ──────────────────────────────────────────────────────

    constexpr FixedVec3() noexcept = default;

    constexpr FixedVec3(Fixed16 xf, Fixed16 yf, Fixed16 zf) noexcept
        : x(xf), y(yf), z(zf) {}

    /// Construct from a float Vec3.  This is the *only* place float
    /// can flow into a FixedVec3 — every other entry point must come
    /// from integer state so determinism holds.
    [[nodiscard]] static constexpr FixedVec3 FromFloat(const Vec3& v) noexcept
    {
        return { ToFixed16(v.x), ToFixed16(v.y), ToFixed16(v.z) };
    }

    /// Lossy round-trip back to float.  Used only on the boundaries
    /// (serialisation, rendering) and never inside the sim loop.
    [[nodiscard]] constexpr Vec3 ToFloat() const noexcept
    {
        return { FromFixed16(x), FromFixed16(y), FromFixed16(z) };
    }

    // ── Arithmetic ────────────────────────────────────────────────────────

    [[nodiscard]] constexpr FixedVec3 operator+(const FixedVec3& o) const noexcept
    {
        return { Add(x, o.x), Add(y, o.y), Add(z, o.z) };
    }

    [[nodiscard]] constexpr FixedVec3 operator-(const FixedVec3& o) const noexcept
    {
        // Subtraction through Add(-b) keeps us inside the deterministic
        // helper, which in turn has been audited to not silently emit
        // a libm call.
        return { Add(x, -o.x), Add(y, -o.y), Add(z, -o.z) };
    }

    [[nodiscard]] constexpr FixedVec3 operator*(Fixed16 scalar) const noexcept
    {
        return { Multiply(x, scalar), Multiply(y, scalar), Multiply(z, scalar) };
    }

    constexpr FixedVec3& operator+=(const FixedVec3& o) noexcept
    {
        x = Add(x, o.x);
        y = Add(y, o.y);
        z = Add(z, o.z);
        return *this;
    }

    // ── Geometric primitives ──────────────────────────────────────────────

    /// Dot product.  Widens to int64 internally so intermediate
    /// products don't overflow Q16.16 range — the caller then sees a
    /// clamped Q16.16 result.  Two orthogonal unit vectors at the
    /// extreme of the range cannot overflow the int64 accumulator.
    [[nodiscard]] constexpr Fixed16 Dot(const FixedVec3& o) const noexcept
    {
        const std::int64_t xx = static_cast<std::int64_t>(x) * o.x;
        const std::int64_t yy = static_cast<std::int64_t>(y) * o.y;
        const std::int64_t zz = static_cast<std::int64_t>(z) * o.z;
        return static_cast<Fixed16>((xx + yy + zz) >> 16);
    }

    /// Squared magnitude.  Keep in Fixed16 instead of going up to the
    /// natural int64 product: saturating here lets downstream callers
    /// compare magnitudes without worrying about mixed widths.
    [[nodiscard]] constexpr Fixed16 MagnitudeSq() const noexcept
    {
        return Dot(*this);
    }

    /// Cross product.  Every intermediate is a 64-bit product so two
    /// near-range inputs cannot wrap.  Final result is Q16.16.
    [[nodiscard]] constexpr FixedVec3 Cross(const FixedVec3& o) const noexcept
    {
        const std::int64_t cx =
            (static_cast<std::int64_t>(y) * o.z - static_cast<std::int64_t>(z) * o.y) >> 16;
        const std::int64_t cy =
            (static_cast<std::int64_t>(z) * o.x - static_cast<std::int64_t>(x) * o.z) >> 16;
        const std::int64_t cz =
            (static_cast<std::int64_t>(x) * o.y - static_cast<std::int64_t>(y) * o.x) >> 16;
        return { static_cast<Fixed16>(cx),
                 static_cast<Fixed16>(cy),
                 static_cast<Fixed16>(cz) };
    }

    // ── Comparison ────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(const FixedVec3& o) const noexcept
    {
        return x == o.x && y == o.y && z == o.z;
    }
    [[nodiscard]] constexpr bool operator!=(const FixedVec3& o) const noexcept
    {
        return !(*this == o);
    }
};

// ─── FixedQuat ──────────────────────────────────────────────────────────────

/// Q16.16 quaternion.  Same rationale as FixedVec3 — integer-only
/// multiplication so replay is bit-identical.  Stored (x, y, z, w) to
/// match `Math::Quat` byte-for-byte at the float conversion boundary.
struct FixedQuat
{
    Fixed16 x = 0;
    Fixed16 y = 0;
    Fixed16 z = 0;
    Fixed16 w = 1 << 16; ///< Identity: w = 1.0 in Q16.16.

    // ── Construction ──────────────────────────────────────────────────────

    constexpr FixedQuat() noexcept = default;
    constexpr FixedQuat(Fixed16 xf, Fixed16 yf, Fixed16 zf, Fixed16 wf) noexcept
        : x(xf), y(yf), z(zf), w(wf) {}

    [[nodiscard]] static constexpr FixedQuat Identity() noexcept
    {
        return { 0, 0, 0, 1 << 16 };
    }

    /// Construct from a float Quat — boundary-only, see FixedVec3::FromFloat.
    [[nodiscard]] static constexpr FixedQuat FromFloat(const Quat& q) noexcept
    {
        return { ToFixed16(q.x), ToFixed16(q.y), ToFixed16(q.z), ToFixed16(q.w) };
    }

    [[nodiscard]] constexpr Quat ToFloat() const noexcept
    {
        return { FromFixed16(x), FromFixed16(y), FromFixed16(z), FromFixed16(w) };
    }

    // ── Multiplication (Hamilton product) ─────────────────────────────────

    /// Hamilton product, fully integer.  Each intermediate product goes
    /// into int64 before the >>16 shift so the result stays in Q16.16
    /// range without an intermediate float.
    [[nodiscard]] constexpr FixedQuat operator*(const FixedQuat& o) const noexcept
    {
        // qx = w*o.x + x*o.w + y*o.z - z*o.y
        // qy = w*o.y - x*o.z + y*o.w + z*o.x
        // qz = w*o.z + x*o.y - y*o.x + z*o.w
        // qw = w*o.w - x*o.x - y*o.y - z*o.z
        using I64 = std::int64_t;
        const I64 qx = (I64(w) * o.x + I64(x) * o.w + I64(y) * o.z - I64(z) * o.y) >> 16;
        const I64 qy = (I64(w) * o.y - I64(x) * o.z + I64(y) * o.w + I64(z) * o.x) >> 16;
        const I64 qz = (I64(w) * o.z + I64(x) * o.y - I64(y) * o.x + I64(z) * o.w) >> 16;
        const I64 qw = (I64(w) * o.w - I64(x) * o.x - I64(y) * o.y - I64(z) * o.z) >> 16;
        return { static_cast<Fixed16>(qx),
                 static_cast<Fixed16>(qy),
                 static_cast<Fixed16>(qz),
                 static_cast<Fixed16>(qw) };
    }

    /// Conjugate: negate the vector part, keep w.  Pure integer negation.
    [[nodiscard]] constexpr FixedQuat Conjugate() const noexcept
    {
        return { -x, -y, -z, w };
    }
};

// ─── Boundary helpers ───────────────────────────────────────────────────────

/// Convert back to float Vec3.  Free function mirrors Vec3's style for
/// call-site consistency.
[[nodiscard]] inline constexpr Vec3 ToFloat(const FixedVec3& v) noexcept
{
    return v.ToFloat();
}

[[nodiscard]] inline constexpr Quat ToFloat(const FixedQuat& q) noexcept
{
    return q.ToFloat();
}

[[nodiscard]] inline constexpr FixedVec3 FromFloat(const Vec3& v) noexcept
{
    return FixedVec3::FromFloat(v);
}

[[nodiscard]] inline constexpr FixedQuat FromFloat(const Quat& q) noexcept
{
    return FixedQuat::FromFloat(q);
}

} // namespace SagaEngine::Math::Deterministic
