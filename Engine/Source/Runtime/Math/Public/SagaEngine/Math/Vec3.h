/// @file Vec3.h
/// @brief Engine math wrapper for a 3-component float vector (GLM backend).
///
/// Purpose: All engine code uses SagaEngine::Math::Vec3 — never raw glm types.
///          This isolates the GLM dependency to the Math module so that other
///          modules (ECS, Replication, Client) stay GLM-free in their own headers.
///
/// Design:
///   - Thin value wrapper: sizeof(Vec3) == sizeof(glm::vec3) == 12 bytes.
///   - All operations are constexpr / noexcept where GLM permits.
///   - Implicit conversion to/from glm::vec3 via explicit helpers only (no
///     implicit constructors) to make GLM boundaries visible at call sites.

#pragma once

#include <cmath>
#include <cstddef>
#include <iosfwd>
#include <string>

namespace SagaEngine::Math
{

// ─── Vec3 ─────────────────────────────────────────────────────────────────────

/// Immutable-by-default 3-component float vector.  Arithmetic operators return
/// new values; mutation is explicit via compound-assignment forms.
struct Vec3
{
    float x{0.0f}; ///< X component (right in engine convention).
    float y{0.0f}; ///< Y component (up in engine convention).
    float z{0.0f}; ///< Z component (forward/back in engine convention).

    // ── Construction ──────────────────────────────────────────────────────────

    constexpr Vec3() noexcept = default;

    constexpr Vec3(float x_, float y_, float z_) noexcept
        : x(x_), y(y_), z(z_) {}

    constexpr explicit Vec3(float scalar) noexcept
        : x(scalar), y(scalar), z(scalar) {}

    // ── Named constructors ────────────────────────────────────────────────────

    [[nodiscard]] static constexpr Vec3 Zero()    noexcept { return {0.0f, 0.0f,  0.0f}; }
    [[nodiscard]] static constexpr Vec3 One()     noexcept { return {1.0f, 1.0f,  1.0f}; }
    [[nodiscard]] static constexpr Vec3 Up()      noexcept { return {0.0f, 1.0f,  0.0f}; }
    [[nodiscard]] static constexpr Vec3 Down()    noexcept { return {0.0f,-1.0f,  0.0f}; }
    [[nodiscard]] static constexpr Vec3 Right()   noexcept { return {1.0f, 0.0f,  0.0f}; }
    [[nodiscard]] static constexpr Vec3 Left()    noexcept { return {-1.0f,0.0f,  0.0f}; }
    [[nodiscard]] static constexpr Vec3 Forward() noexcept { return {0.0f, 0.0f, -1.0f}; }
    [[nodiscard]] static constexpr Vec3 Back()    noexcept { return {0.0f, 0.0f,  1.0f}; }

    // ── Arithmetic operators ──────────────────────────────────────────────────

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& o) const noexcept
    {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr Vec3 operator-(const Vec3& o) const noexcept
    {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr Vec3 operator*(float s) const noexcept
    {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] constexpr Vec3 operator/(float s) const noexcept
    {
        return {x / s, y / s, z / s};
    }

    [[nodiscard]] constexpr Vec3 operator-() const noexcept
    {
        return {-x, -y, -z};
    }

    constexpr Vec3& operator+=(const Vec3& o) noexcept
    {
        x += o.x; y += o.y; z += o.z; return *this;
    }

    constexpr Vec3& operator-=(const Vec3& o) noexcept
    {
        x -= o.x; y -= o.y; z -= o.z; return *this;
    }

    constexpr Vec3& operator*=(float s) noexcept
    {
        x *= s; y *= s; z *= s; return *this;
    }

    constexpr Vec3& operator/=(float s) noexcept
    {
        x /= s; y /= s; z /= s; return *this;
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(const Vec3& o) const noexcept
    {
        return x == o.x && y == o.y && z == o.z;
    }

    [[nodiscard]] constexpr bool operator!=(const Vec3& o) const noexcept
    {
        return !(*this == o);
    }

    // ── Vector math ───────────────────────────────────────────────────────────

    /// Return the squared length — avoids a sqrt, prefer for distance comparisons.
    [[nodiscard]] constexpr float LengthSq() const noexcept
    {
        return x * x + y * y + z * z;
    }

    /// Return the Euclidean length.
    [[nodiscard]] float Length() const noexcept
    {
        return std::sqrt(LengthSq());
    }

    /// Return a unit-length copy.  Behaviour is undefined if length is zero.
    [[nodiscard]] Vec3 Normalized() const noexcept
    {
        const float len = Length();
        return {x / len, y / len, z / len};
    }

    /// Dot product with another vector.
    [[nodiscard]] constexpr float Dot(const Vec3& o) const noexcept
    {
        return x * o.x + y * o.y + z * o.z;
    }

    /// Cross product — right-hand rule.
    [[nodiscard]] constexpr Vec3 Cross(const Vec3& o) const noexcept
    {
        return { y * o.z - z * o.y,
                 z * o.x - x * o.z,
                 x * o.y - y * o.x };
    }

    /// Euclidean distance to another point.
    [[nodiscard]] float DistanceTo(const Vec3& o) const noexcept
    {
        return (*this - o).Length();
    }

    /// Squared Euclidean distance — avoids sqrt, prefer for spatial comparisons.
    [[nodiscard]] constexpr float DistanceSqTo(const Vec3& o) const noexcept
    {
        return (*this - o).LengthSq();
    }

    // ── Linear interpolation ──────────────────────────────────────────────────

    /// Lerp towards `target` by `t` in [0, 1].
    [[nodiscard]] constexpr Vec3 Lerp(const Vec3& target, float t) const noexcept
    {
        return {x + (target.x - x) * t,
                y + (target.y - y) * t,
                z + (target.z - z) * t};
    }

    // ── Component access ──────────────────────────────────────────────────────

    [[nodiscard]] constexpr float  operator[](std::size_t i) const noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr float& operator[](std::size_t i)       noexcept { return (&x)[i]; }

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /// Return a human-readable representation, e.g. "(1.00, 2.50, -0.10)".
    [[nodiscard]] std::string ToString() const;
};

// ── Free-function scalar * Vec3 ───────────────────────────────────────────────

[[nodiscard]] inline constexpr Vec3 operator*(float s, const Vec3& v) noexcept
{
    return v * s;
}

// ── Free-function utilities ───────────────────────────────────────────────────

/// Component-wise minimum.
[[nodiscard]] inline constexpr Vec3 Min(const Vec3& a, const Vec3& b) noexcept
{
    return { a.x < b.x ? a.x : b.x,
             a.y < b.y ? a.y : b.y,
             a.z < b.z ? a.z : b.z };
}

/// Component-wise maximum.
[[nodiscard]] inline constexpr Vec3 Max(const Vec3& a, const Vec3& b) noexcept
{
    return { a.x > b.x ? a.x : b.x,
             a.y > b.y ? a.y : b.y,
             a.z > b.z ? a.z : b.z };
}

/// Clamp each component to [lo, hi].
[[nodiscard]] inline constexpr Vec3 Clamp(const Vec3& v,
                                          const Vec3& lo,
                                          const Vec3& hi) noexcept
{
    return Min(Max(v, lo), hi);
}

} // namespace SagaEngine::Math
