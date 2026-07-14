/// @file GizmoHandle.cpp
/// @brief Implementation of gizmo handle identity and hit-testing.

#include "SagaEditor/Viewport/GizmoHandle.h"

#include <cmath>

namespace SagaEditor
{

namespace
{

using Vec3 = SagaEngine::Math::Vec3;

// ─── Local Helpers ────────────────────────────────────────────────────────────

[[nodiscard]] inline Vec3 AxisVector(int index) noexcept
{
    switch (index)
    {
        case 0: return Vec3{1.0f, 0.0f, 0.0f};
        case 1: return Vec3{0.0f, 1.0f, 0.0f};
        default: return Vec3{0.0f, 0.0f, 1.0f};
    }
}

[[nodiscard]] inline GizmoHandle TranslateAxisHandle(int axis) noexcept
{
    switch (axis)
    {
        case 0: return GizmoHandle::AxisX;
        case 1: return GizmoHandle::AxisY;
        default: return GizmoHandle::AxisZ;
    }
}

[[nodiscard]] inline GizmoHandle RotationAxisHandle(int axis) noexcept
{
    switch (axis)
    {
        case 0: return GizmoHandle::RotationX;
        case 1: return GizmoHandle::RotationY;
        default: return GizmoHandle::RotationZ;
    }
}

/// Try the three axis arrows of a translate / scale gizmo. Returns
/// the best hit (smallest non-negative ray-t) or nullopt.
[[nodiscard]] std::optional<GizmoHandleHit>
HitTestAxes(const ViewportRay&    ray,
            const Vec3&           origin,
            float                 length,
            float                 tolerance) noexcept
{
    std::optional<GizmoHandleHit> best;
    const float toleranceSq = tolerance * tolerance;

    for (int axis = 0; axis < 3; ++axis)
    {
        const Vec3 a = origin;
        const Vec3 b{ origin.x + AxisVector(axis).x * length,
                      origin.y + AxisVector(axis).y * length,
                      origin.z + AxisVector(axis).z * length };

        const auto closest = ClosestRayToSegment(ray, a, b);
        if (!closest.has_value())
        {
            continue;
        }
        if (closest->t < 0.0f)
        {
            continue;
        }
        if (closest->distSq > toleranceSq)
        {
            continue;
        }
        if (!best.has_value() || closest->t < best->rayT)
        {
            GizmoHandleHit hit;
            hit.handle   = TranslateAxisHandle(axis);
            hit.rayT     = closest->t;
            hit.distance = std::sqrt(closest->distSq);
            best         = hit;
        }
    }

    return best;
}

/// Try the three plane handles of the translate gizmo. Each handle
/// is a square in the plane defined by two of the world axes.
[[nodiscard]] std::optional<GizmoHandleHit>
HitTestPlanes(const ViewportRay&    ray,
              const Vec3&           origin,
              float                 halfExtent,
              float                 tolerance) noexcept
{
    struct PlaneSpec
    {
        GizmoHandle handle;
        Vec3        normal;
        Vec3        u; // first in-plane axis (we measure local |u| coord)
        Vec3        v; // second in-plane axis
    };
    const PlaneSpec planes[] = {
        { GizmoHandle::PlaneXY, Vec3{0.0f, 0.0f, 1.0f},
          Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f} },
        { GizmoHandle::PlaneYZ, Vec3{1.0f, 0.0f, 0.0f},
          Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f} },
        { GizmoHandle::PlaneXZ, Vec3{0.0f, 1.0f, 0.0f},
          Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f} },
    };

    std::optional<GizmoHandleHit> best;

    for (const auto& p : planes)
    {
        const auto t = IntersectRayPlane(ray, origin, p.normal);
        if (!t.has_value() || *t < 0.0f)
        {
            continue;
        }
        const Vec3 hitPoint = ray.PointAt(*t);
        const Vec3 local{
            hitPoint.x - origin.x,
            hitPoint.y - origin.y,
            hitPoint.z - origin.z,
        };

        // The handle quad spans `[0, halfExtent*2]` along each
        // in-plane axis with one corner anchored at the gizmo origin
        // — this matches how the renderer draws the quad (between the
        // two arrow shafts) so picking lines up with the visual.
        const float coordU = local.Dot(p.u);
        const float coordV = local.Dot(p.v);
        if (coordU < -tolerance || coordU > halfExtent * 2.0f + tolerance) continue;
        if (coordV < -tolerance || coordV > halfExtent * 2.0f + tolerance) continue;

        if (!best.has_value() || *t < best->rayT)
        {
            GizmoHandleHit hit;
            hit.handle   = p.handle;
            hit.rayT     = *t;
            hit.distance = 0.0f; // a plane hit lies exactly on the plane
            best         = hit;
        }
    }

    return best;
}

[[nodiscard]] std::optional<GizmoHandleHit>
PickBetween(std::optional<GizmoHandleHit> a,
            std::optional<GizmoHandleHit> b) noexcept
{
    if (!a.has_value()) return b;
    if (!b.has_value()) return a;
    return (a->rayT <= b->rayT) ? a : b;
}

} // namespace

// ─── Identity ─────────────────────────────────────────────────────────────────

const char* GizmoHandleId(GizmoHandle handle) noexcept
{
    switch (handle)
    {
        case GizmoHandle::None:         return "";
        case GizmoHandle::AxisX:        return "saga.gizmo.axis.x";
        case GizmoHandle::AxisY:        return "saga.gizmo.axis.y";
        case GizmoHandle::AxisZ:        return "saga.gizmo.axis.z";
        case GizmoHandle::PlaneXY:      return "saga.gizmo.plane.xy";
        case GizmoHandle::PlaneYZ:      return "saga.gizmo.plane.yz";
        case GizmoHandle::PlaneXZ:      return "saga.gizmo.plane.xz";
        case GizmoHandle::RotationX:    return "saga.gizmo.rot.x";
        case GizmoHandle::RotationY:    return "saga.gizmo.rot.y";
        case GizmoHandle::RotationZ:    return "saga.gizmo.rot.z";
        case GizmoHandle::ScaleUniform: return "saga.gizmo.scale.uniform";
    }
    return "";
}

// ─── Translate Gizmo ──────────────────────────────────────────────────────────

std::optional<GizmoHandleHit>
HitTestTranslateGizmo(const ViewportRay&    ray,
                      const Vec3&           gizmoOrigin,
                      const GizmoHitConfig& config) noexcept
{
    auto axes   = HitTestAxes  (ray, gizmoOrigin,
                                config.axisLength,
                                config.axisHitTolerance);
    auto planes = HitTestPlanes(ray, gizmoOrigin,
                                config.planeHalfExtent,
                                config.planeHitTolerance);
    return PickBetween(axes, planes);
}

// ─── Rotate Gizmo ─────────────────────────────────────────────────────────────

std::optional<GizmoHandleHit>
HitTestRotateGizmo(const ViewportRay&    ray,
                   const Vec3&           gizmoOrigin,
                   const GizmoHitConfig& config) noexcept
{
    std::optional<GizmoHandleHit> best;
    const float ringRadius = config.axisLength;
    const float tolerance  = config.axisHitTolerance;

    for (int axis = 0; axis < 3; ++axis)
    {
        const Vec3 normal = AxisVector(axis);
        const auto t      = IntersectRayPlane(ray, gizmoOrigin, normal);
        if (!t.has_value() || *t < 0.0f)
        {
            continue;
        }
        const Vec3 hitPoint = ray.PointAt(*t);
        const Vec3 inPlane{
            hitPoint.x - gizmoOrigin.x,
            hitPoint.y - gizmoOrigin.y,
            hitPoint.z - gizmoOrigin.z,
        };

        const float radius = std::sqrt(inPlane.LengthSq());
        const float delta  = std::fabs(radius - ringRadius);
        if (delta > tolerance)
        {
            continue;
        }

        if (!best.has_value() || *t < best->rayT)
        {
            GizmoHandleHit hit;
            hit.handle   = RotationAxisHandle(axis);
            hit.rayT     = *t;
            hit.distance = delta;
            best         = hit;
        }
    }

    return best;
}

// ─── Scale Gizmo ──────────────────────────────────────────────────────────────

std::optional<GizmoHandleHit>
HitTestScaleGizmo(const ViewportRay&    ray,
                  const Vec3&           gizmoOrigin,
                  const GizmoHitConfig& config) noexcept
{
    auto axes = HitTestAxes(ray, gizmoOrigin,
                            config.axisLength,
                            config.axisHitTolerance);

    // Uniform-scale handle is a sphere at the origin sized roughly
    // 1.5× the axis hit tolerance so the user can grab it without
    // pixel-perfect aim.
    const float r = config.axisHitTolerance * 1.5f;
    auto sphereT = IntersectRaySphere(ray, gizmoOrigin, r);
    if (sphereT.has_value() && *sphereT >= 0.0f)
    {
        GizmoHandleHit centre;
        centre.handle   = GizmoHandle::ScaleUniform;
        centre.rayT     = *sphereT;
        centre.distance = 0.0f;
        if (!axes.has_value() || centre.rayT < axes->rayT)
        {
            return centre;
        }
    }

    return axes;
}

} // namespace SagaEditor
