/// @file ViewportRay.h
/// @brief Screen-pixel-to-world-ray construction and primitive intersection.

#pragma once

#include "SagaEditor/Viewport/ViewportCamera.h"

#include "SagaEngine/Math/Vec3.h"

#include <optional>

namespace SagaEditor
{

// ─── Ray Type ─────────────────────────────────────────────────────────────────

/// World-space ray with a unit-length direction. The editor uses the
/// same ray for selection picking, gizmo handle hit-testing, and
/// transform-snap raycasts; keeping a single value type means the
/// ray-vs-primitive helpers below have one signature.
struct ViewportRay
{
    using Vec3 = SagaEngine::Math::Vec3;

    Vec3 origin{};      ///< World-space origin (typically the camera position).
    Vec3 direction{};   ///< World-space unit-length direction.

    /// Convenience: point along the ray at parameter `t`. Negative `t`
    /// is allowed and yields a point behind the ray origin.
    [[nodiscard]] Vec3 PointAt(float t) const noexcept
    {
        return Vec3{ origin.x + direction.x * t,
                     origin.y + direction.y * t,
                     origin.z + direction.z * t };
    }
};

// ─── Screen-to-World Ray ──────────────────────────────────────────────────────

/// Build a world-space ray from a panel-local pixel coordinate.
///
/// `pixelX`, `pixelY` are panel-local with origin at the top-left
/// (`pixelY` increases downward, matching the input snapshot). The
/// returned ray has its origin at the camera position and a
/// unit-length direction pointing through the supplied pixel.
///
/// `viewportWidth` / `viewportHeight` must both be positive — the
/// helper returns a degenerate ray (origin at camera, direction = camera
/// forward) when either dimension is zero so a caller hitting an
/// unrendered panel does not have to special-case it.
///
/// In orthographic mode, the ray's origin is offset out of the camera
/// position into the orthographic plane so that intersection tests
/// behave the same way they do in perspective mode (i.e. the same
/// `IntersectPlane` call works regardless of projection).
[[nodiscard]] ViewportRay ScreenToWorldRay(const ViewportCamera& camera,
                                           float                 pixelX,
                                           float                 pixelY,
                                           float                 viewportWidth,
                                           float                 viewportHeight) noexcept;

// ─── Primitive Intersection ───────────────────────────────────────────────────

/// Intersect `ray` with the plane defined by point `planePoint` and
/// unit normal `planeNormal`. Returns the parametric `t` at which the
/// ray meets the plane, or `std::nullopt` when the ray is parallel to
/// the plane within a small epsilon. Negative `t` (intersection
/// behind the origin) is preserved so callers can decide whether to
/// reject behind-the-camera hits explicitly.
[[nodiscard]] std::optional<float>
    IntersectRayPlane(const ViewportRay&             ray,
                      const SagaEngine::Math::Vec3&  planePoint,
                      const SagaEngine::Math::Vec3&  planeNormal) noexcept;

/// Intersect `ray` with the sphere of `radius` centred at `centre`.
/// Returns the smallest non-negative `t` of the two intersection
/// points, or `std::nullopt` if the ray misses the sphere. Used by
/// the editor for sphere-bounded entity picks before falling back to
/// a more expensive mesh raycast.
[[nodiscard]] std::optional<float>
    IntersectRaySphere(const ViewportRay&             ray,
                       const SagaEngine::Math::Vec3&  centre,
                       float                          radius) noexcept;

/// Closest pair of points between `ray` and the line segment
/// `(a, b)`. Returns `std::nullopt` when the ray and the segment are
/// parallel (the closest distance is then any point along the
/// segment). The returned struct carries the segment-space `s`
/// parameter (clamped to `[0, 1]`), the ray-space `t` parameter
/// (unclamped), and the squared distance between the closest pair.
struct RaySegmentClosest
{
    float t        = 0.0f; ///< Ray parameter at the closest point.
    float s        = 0.0f; ///< Segment parameter, clamped to [0, 1].
    float distSq   = 0.0f; ///< Squared distance between the two closest points.
};

[[nodiscard]] std::optional<RaySegmentClosest>
    ClosestRayToSegment(const ViewportRay&             ray,
                        const SagaEngine::Math::Vec3&  a,
                        const SagaEngine::Math::Vec3&  b) noexcept;

} // namespace SagaEditor
