/// @file Transform.h
/// @brief Composite position / rotation / scale value type for scene objects.
///
/// Purpose: Transform is the standard way to represent where an object lives
///          in 3D world space.  It replaces any ad-hoc "pos + rot + scale"
///          struct that might otherwise appear in ECS components or network
///          snapshots.  All engine subsystems (ECS, replication, client
///          interpolation) use this type as the common currency.
///
/// Design:
///   - Plain value type — no virtual, no heap, 40 bytes total.
///   - Combines SagaEngine::Math::Vec3 (position, scale) and Quat (rotation).
///   - Matrix conversion helpers are free functions to keep the type lightweight.
///   - Interpolation helpers are provided for the client prediction pipeline.

#pragma once

#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Math/Quat.h"

#include <string>

namespace SagaEngine::Math
{

// ─── Transform ────────────────────────────────────────────────────────────────

/// Position, orientation, and uniform / non-uniform scale in world or local space.
struct Transform
{
    Vec3 position{Vec3::Zero()};  ///< World position in metres.
    Quat rotation{Quat::Identity()};  ///< Orientation as a unit quaternion.
    Vec3 scale{Vec3::One()};      ///< Per-axis scale factor (1 = no scaling).

    // ── Construction ──────────────────────────────────────────────────────────

    constexpr Transform() noexcept = default;

    constexpr Transform(const Vec3& pos,
                        const Quat& rot,
                        const Vec3& scl) noexcept
        : position(pos), rotation(rot), scale(scl) {}

    /// Construct a translation-only transform (identity rotation, unit scale).
    static constexpr Transform FromPosition(const Vec3& pos) noexcept
    {
        return {pos, Quat::Identity(), Vec3::One()};
    }

    /// Construct from position and rotation (unit scale).
    static constexpr Transform FromPositionRotation(const Vec3& pos,
                                                     const Quat& rot) noexcept
    {
        return {pos, rot, Vec3::One()};
    }

    // ── Identity ──────────────────────────────────────────────────────────────

    /// Return an identity transform: origin, no rotation, unit scale.
    [[nodiscard]] static constexpr Transform Identity() noexcept
    {
        return {};
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr bool operator==(const Transform& o) const noexcept
    {
        return position == o.position &&
               rotation == o.rotation &&
               scale    == o.scale;
    }

    [[nodiscard]] constexpr bool operator!=(const Transform& o) const noexcept
    {
        return !(*this == o);
    }

    // ── Point transformation ──────────────────────────────────────────────────

    /// Transform a local-space point to world space: scale → rotate → translate.
    [[nodiscard]] Vec3 TransformPoint(const Vec3& localPoint) const noexcept
    {
        const Vec3 scaled   = {localPoint.x * scale.x,
                               localPoint.y * scale.y,
                               localPoint.z * scale.z};
        const Vec3 rotated  = rotation.Rotate(scaled);
        return rotated + position;
    }

    /// Transform a local-space direction (no translation, no scale).
    [[nodiscard]] Vec3 TransformDirection(const Vec3& localDir) const noexcept
    {
        return rotation.Rotate(localDir);
    }

    // ── Combination ───────────────────────────────────────────────────────────

    /// Compose with a child transform (parent.CombineWith(child)).
    /// The child is expressed in the parent's local space.
    [[nodiscard]] Transform CombineWith(const Transform& child) const noexcept
    {
        Transform result;
        result.scale    = {scale.x * child.scale.x,
                           scale.y * child.scale.y,
                           scale.z * child.scale.z};
        result.rotation = rotation * child.rotation;
        result.position = TransformPoint(child.position);
        return result;
    }

    /// Return the inverse transform (undo position, rotation, scale).
    [[nodiscard]] Transform Inverse() const noexcept
    {
        Transform inv;
        inv.rotation = rotation.Conjugate();
        inv.scale    = {1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z};
        inv.position = inv.rotation.Rotate(
            {-position.x * inv.scale.x,
             -position.y * inv.scale.y,
             -position.z * inv.scale.z});
        return inv;
    }

    // ── Interpolation ─────────────────────────────────────────────────────────

    /// Lerp position, Nlerp rotation, Lerp scale.
    /// Used by the client interpolation buffer between two received snapshots.
    [[nodiscard]] Transform Lerp(const Transform& target, float t) const noexcept
    {
        return { position.Lerp(target.position, t),
                 rotation.Nlerp(target.rotation, t),
                 scale.Lerp(target.scale, t) };
    }

    /// Slerp rotation, Lerp position and scale — higher quality, more expensive.
    [[nodiscard]] Transform Slerp(const Transform& target, float t) const noexcept
    {
        return { position.Lerp(target.position, t),
                 rotation.Slerp(target.rotation, t),
                 scale.Lerp(target.scale, t) };
    }

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /// Return a human-readable summary for logging.
    [[nodiscard]] std::string ToString() const;
};

} // namespace SagaEngine::Math
