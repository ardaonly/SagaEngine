/// @file Quat.h
/// @brief Engine math wrapper for a unit quaternion (GLM backend).
///
/// Purpose: Quaternions represent 3D rotations without gimbal lock.
///          SagaEngine code must use this wrapper — never raw glm::quat — so
///          that the GLM boundary stays confined to the Math module.
///
/// Convention: unit quaternion, XYZW component order in memory.
///   w = cos(θ/2), xyz = sin(θ/2) * axis.
///   All engine rotations are stored normalised unless marked otherwise.

#pragma once

#include "SagaEngine/Math/Vec3.h"

#include <cmath>
#include <string>

namespace SagaEngine::Math
{

// ─── Quat ─────────────────────────────────────────────────────────────────────

/// Unit quaternion representing a 3D rotation.
/// Store as XYZW in memory (matches GLM internal layout).
struct Quat
{
    float x{0.0f}; ///< Imaginary i component.
    float y{0.0f}; ///< Imaginary j component.
    float z{0.0f}; ///< Imaginary k component.
    float w{1.0f}; ///< Real component.  Identity has w=1.

    // ── Construction ──────────────────────────────────────────────────────────

    constexpr Quat() noexcept = default;

    constexpr Quat(float x_, float y_, float z_, float w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}

    // ── Named constructors ────────────────────────────────────────────────────

    /// Identity rotation — no rotation applied.
    [[nodiscard]] static constexpr Quat Identity() noexcept
    {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }

    /// Construct from an axis (unit vector) and angle in radians.
    [[nodiscard]] static Quat FromAxisAngle(const Vec3& axis, float angleRadians) noexcept;

    /// Construct from Euler angles (pitch/yaw/roll) in radians, applied YXZ order.
    [[nodiscard]] static Quat FromEuler(float pitchRad,
                                        float yawRad,
                                        float rollRad) noexcept;

    // ── Hamilton product ──────────────────────────────────────────────────────

    /// Compose two rotations: apply `this` first, then `o`.
    [[nodiscard]] constexpr Quat operator*(const Quat& o) const noexcept
    {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    constexpr Quat& operator*=(const Quat& o) noexcept
    {
        *this = *this * o;
        return *this;
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(const Quat& o) const noexcept
    {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }

    [[nodiscard]] constexpr bool operator!=(const Quat& o) const noexcept
    {
        return !(*this == o);
    }

    // ── Core operations ───────────────────────────────────────────────────────

    /// Squared norm.  For a unit quaternion this should be 1.0.
    [[nodiscard]] constexpr float NormSq() const noexcept
    {
        return x * x + y * y + z * z + w * w;
    }

    /// Norm (magnitude).
    [[nodiscard]] float Norm() const noexcept
    {
        return std::sqrt(NormSq());
    }

    /// Return a normalised copy.
    [[nodiscard]] Quat Normalized() const noexcept;

    /// Conjugate — inverse rotation for unit quaternions.
    [[nodiscard]] constexpr Quat Conjugate() const noexcept
    {
        return {-x, -y, -z, w};
    }

    /// Inverse (works for non-unit quaternions; prefer Conjugate for unit quats).
    [[nodiscard]] Quat Inverse() const noexcept;

    /// Dot product (used for angle measurement and SLERP).
    [[nodiscard]] constexpr float Dot(const Quat& o) const noexcept
    {
        return x * o.x + y * o.y + z * o.z + w * o.w;
    }

    // ── Rotation application ──────────────────────────────────────────────────

    /// Rotate a vector by this quaternion.
    [[nodiscard]] Vec3 Rotate(const Vec3& v) const noexcept;

    // ── Decomposition ─────────────────────────────────────────────────────────

    /// Extract Euler angles (pitch, yaw, roll) in radians.
    [[nodiscard]] Vec3 ToEulerAngles() const noexcept;

    /// Extract the rotation axis (unit vector) and angle in radians.
    void ToAxisAngle(Vec3& outAxis, float& outAngleRad) const noexcept;

    // ── Interpolation ─────────────────────────────────────────────────────────

    /// Normalised linear interpolation — fast, slightly non-constant velocity.
    [[nodiscard]] Quat Nlerp(const Quat& target, float t) const noexcept;

    /// Spherical linear interpolation — constant angular velocity.
    [[nodiscard]] Quat Slerp(const Quat& target, float t) const noexcept;

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /// Return a human-readable representation, e.g. "Quat(x=0.00, y=0.00, z=0.00, w=1.00)".
    [[nodiscard]] std::string ToString() const;
};

} // namespace SagaEngine::Math
