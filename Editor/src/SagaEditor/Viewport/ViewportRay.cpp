/// @file ViewportRay.cpp
/// @brief Implementation of viewport ray construction and intersection helpers.

#include "SagaEditor/Viewport/ViewportRay.h"

#include <algorithm>
#include <cmath>

namespace SagaEditor
{

namespace
{

// ─── Local Constants ──────────────────────────────────────────────────────────

/// Threshold below which a denominator is treated as zero. Calibrated
/// against single-precision floats; tighter epsilons start producing
/// false rejections under near-grazing rays.
constexpr float kRayEpsilon = 1.0e-6f;

using Vec3 = SagaEngine::Math::Vec3;

[[nodiscard]] inline Vec3 SafeNormalized(const Vec3& v, const Vec3& fallback) noexcept
{
    const float lenSq = v.LengthSq();
    if (lenSq < kRayEpsilon)
    {
        return fallback;
    }
    const float inv = 1.0f / std::sqrt(lenSq);
    return Vec3{ v.x * inv, v.y * inv, v.z * inv };
}

} // namespace

// ─── Screen-to-World Ray ──────────────────────────────────────────────────────

ViewportRay ScreenToWorldRay(const ViewportCamera& camera,
                             float                 pixelX,
                             float                 pixelY,
                             float                 viewportWidth,
                             float                 viewportHeight) noexcept
{
    ViewportRay ray;

    // Camera basis is recomputed on demand by the value type.
    const Vec3 fwd   = camera.GetForward();
    const Vec3 right = camera.GetRight();
    const Vec3 up    = camera.GetUp();
    const Vec3 pos   = camera.GetPosition();

    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f)
    {
        // Degenerate viewport — return a ray that points straight ahead.
        ray.origin    = pos;
        ray.direction = SafeNormalized(fwd, Vec3{0.0f, 0.0f, -1.0f});
        return ray;
    }

    // Convert pixel coordinates to normalised device coordinates in
    // [-1, +1] with +X right and +Y up. The input convention is panel-
    // local with +Y down, so we flip Y on the way in.
    const float ndcX = (pixelX / viewportWidth)  * 2.0f - 1.0f;
    const float ndcY = 1.0f - (pixelY / viewportHeight) * 2.0f;

    const float aspect = viewportWidth / viewportHeight;

    if (camera.mode == ProjectionMode::Perspective)
    {
        // Camera-space direction at unit forward distance.
        const float halfFovY = camera.fovYRadians * 0.5f;
        const float tanFov   = std::tan(halfFovY);
        const float dx       = ndcX * tanFov * aspect;
        const float dy       = ndcY * tanFov;

        const Vec3 worldDir{
            right.x * dx + up.x * dy + fwd.x,
            right.y * dx + up.y * dy + fwd.y,
            right.z * dx + up.z * dy + fwd.z,
        };

        ray.origin    = pos;
        ray.direction = SafeNormalized(worldDir, fwd);
    }
    else
    {
        // Orthographic: every screen pixel maps to a world-space point
        // on the camera's near plane, and the ray direction equals the
        // camera forward.
        const float halfH = camera.orthoHalfHeight;
        const float halfW = halfH * aspect;
        const float ox    = ndcX * halfW;
        const float oy    = ndcY * halfH;

        const Vec3 origin{
            pos.x + right.x * ox + up.x * oy,
            pos.y + right.y * ox + up.y * oy,
            pos.z + right.z * ox + up.z * oy,
        };

        ray.origin    = origin;
        ray.direction = SafeNormalized(fwd, Vec3{0.0f, 0.0f, -1.0f});
    }

    return ray;
}

// ─── Plane Intersection ───────────────────────────────────────────────────────

std::optional<float>
IntersectRayPlane(const ViewportRay& ray,
                  const Vec3&        planePoint,
                  const Vec3&        planeNormal) noexcept
{
    const float denom = ray.direction.Dot(planeNormal);
    if (std::fabs(denom) < kRayEpsilon)
    {
        return std::nullopt;
    }

    const Vec3 toPlane{
        planePoint.x - ray.origin.x,
        planePoint.y - ray.origin.y,
        planePoint.z - ray.origin.z,
    };
    return toPlane.Dot(planeNormal) / denom;
}

// ─── Sphere Intersection ──────────────────────────────────────────────────────

std::optional<float>
IntersectRaySphere(const ViewportRay& ray,
                   const Vec3&        centre,
                   float              radius) noexcept
{
    if (!(radius > 0.0f))
    {
        return std::nullopt;
    }

    const Vec3 oc{
        ray.origin.x - centre.x,
        ray.origin.y - centre.y,
        ray.origin.z - centre.z,
    };

    // Quadratic coefficients for `||o + t·d - c||² = r²` with d
    // unit-length: t² + 2·(d·oc)·t + (oc·oc - r²) = 0.
    const float b   = ray.direction.Dot(oc);
    const float c   = oc.Dot(oc) - radius * radius;
    const float det = b * b - c;
    if (det < 0.0f)
    {
        return std::nullopt;
    }

    const float sqrtDet = std::sqrt(det);
    const float tNear   = -b - sqrtDet;
    const float tFar    = -b + sqrtDet;

    if (tNear >= 0.0f) return tNear;
    if (tFar  >= 0.0f) return tFar;
    return std::nullopt;
}

// ─── Closest Point Ray ↔ Segment ──────────────────────────────────────────────

std::optional<RaySegmentClosest>
ClosestRayToSegment(const ViewportRay& ray,
                    const Vec3&        a,
                    const Vec3&        b) noexcept
{
    // Parametric forms:
    //   ray   : R(t) = ray.origin + t * ray.direction      (t in R)
    //   seg   : S(s) = a + s * (b - a)                     (s in [0,1])
    //
    // Closest pair satisfies the system:
    //   (1) (R(t) - S(s)) · ray.direction = 0
    //   (2) (R(t) - S(s)) · (b - a)       = 0
    //
    // Following the standard derivation (Eberly et al.), with
    //   d1 = ray.direction, d2 = b - a, w0 = ray.origin - a:
    //   a = d1·d1 = 1 (unit ray dir)
    //   b = d1·d2
    //   c = d2·d2
    //   d = d1·w0
    //   e = d2·w0
    //   denom = a*c - b*b

    const Vec3 d2{ b.x - a.x, b.y - a.y, b.z - a.z };
    const Vec3 w0{ ray.origin.x - a.x,
                   ray.origin.y - a.y,
                   ray.origin.z - a.z };

    const float aa    = ray.direction.Dot(ray.direction); // ≈ 1
    const float bb    = ray.direction.Dot(d2);
    const float cc    = d2.Dot(d2);
    const float dd    = ray.direction.Dot(w0);
    const float ee    = d2.Dot(w0);
    const float denom = aa * cc - bb * bb;

    if (cc < kRayEpsilon || denom < kRayEpsilon)
    {
        // Segment is degenerate or parallel to the ray.
        return std::nullopt;
    }

    const float t = (bb * ee - cc * dd) / denom;
    float       s = (aa * ee - bb * dd) / denom;
    s = std::clamp(s, 0.0f, 1.0f);

    // Re-evaluate ray parameter against the clamped segment point so
    // the reported distance reflects the actual closest pair.
    const float tFromClamped = (s * bb - dd) / aa;

    const Vec3 closestRay{
        ray.origin.x + ray.direction.x * tFromClamped,
        ray.origin.y + ray.direction.y * tFromClamped,
        ray.origin.z + ray.direction.z * tFromClamped,
    };
    const Vec3 closestSeg{
        a.x + d2.x * s,
        a.y + d2.y * s,
        a.z + d2.z * s,
    };
    const Vec3 diff{
        closestRay.x - closestSeg.x,
        closestRay.y - closestSeg.y,
        closestRay.z - closestSeg.z,
    };

    RaySegmentClosest result;
    result.t      = tFromClamped;
    result.s      = s;
    result.distSq = diff.LengthSq();
    (void)t;
    return result;
}

} // namespace SagaEditor
