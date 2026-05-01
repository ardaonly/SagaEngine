/// @file ViewportCamera.cpp
/// @brief Implementation of the editor viewport camera value type.

#include "SagaEditor/Viewport/ViewportCamera.h"

#include "SagaEngine/Math/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace SagaEditor
{

namespace
{

// ─── Local Constants ──────────────────────────────────────────────────────────

/// Tightest pitch the orbit / fly-cam controllers allow. Keeping a
/// small epsilon away from `pi/2` prevents the world-up cross product
/// from collapsing to zero when the camera looks straight up or down.
constexpr double kPitchLimitRadians = 1.5707963 - 1.0e-3;

/// Smallest fov a perspective camera is allowed to drop to (≈ 1°).
constexpr float kMinFovYRadians = 0.0174533f;

/// Largest fov a perspective camera is allowed to use (≈ 170°).
constexpr float kMaxFovYRadians = 2.9670597f;

/// Smallest world-space distance between near and far plane — keeps
/// the depth buffer usable.
constexpr float kMinClipSeparation = 1.0e-3f;

[[nodiscard]] inline double ClampPitch(double pitch) noexcept
{
    if (pitch >  kPitchLimitRadians) return  kPitchLimitRadians;
    if (pitch < -kPitchLimitRadians) return -kPitchLimitRadians;
    return pitch;
}

} // namespace

// ─── Derived Camera Basis ─────────────────────────────────────────────────────

ViewportCamera::Vec3 ViewportCamera::GetForward() const noexcept
{
    // Right-handed system with default forward = -Z.
    // Yaw rotates around +Y; pitch rotates around the camera's right axis.
    const double cy = std::cos(yawRadians);
    const double sy = std::sin(yawRadians);
    const double cp = std::cos(pitchRadians);
    const double sp = std::sin(pitchRadians);

    // Forward when (yaw, pitch) == (0, 0) is (0, 0, -1).
    const double fx = -sy * cp;
    const double fy =  sp;
    const double fz = -cy * cp;

    return Vec3{ static_cast<float>(fx),
                 static_cast<float>(fy),
                 static_cast<float>(fz) };
}

ViewportCamera::Vec3 ViewportCamera::GetRight() const noexcept
{
    // Right is yaw rotation of world +X; ignores pitch so dragging
    // the camera up and down does not roll the right axis out of the
    // XZ plane.
    const double cy = std::cos(yawRadians);
    const double sy = std::sin(yawRadians);

    return Vec3{ static_cast<float>( cy),
                 0.0f,
                 static_cast<float>(-sy) };
}

ViewportCamera::Vec3 ViewportCamera::GetUp() const noexcept
{
    // Recompute from forward × right so the basis is always orthogonal
    // even after the controller has clamped pitch on the way in.
    const Vec3 f = GetForward();
    const Vec3 r = GetRight();
    return r.Cross(f).Normalized();
}

ViewportCamera::Vec3 ViewportCamera::GetPosition() const noexcept
{
    // Camera sits behind the pivot along the *negative* forward
    // direction at distance `orbitRadius`; the pivot is what the
    // camera is looking at.
    const Vec3 f = GetForward();
    return Vec3{ pivot.x - f.x * orbitRadius,
                 pivot.y - f.y * orbitRadius,
                 pivot.z - f.z * orbitRadius };
}

// ─── State Queries ────────────────────────────────────────────────────────────

double ViewportCamera::GetYawDegrees() const noexcept
{
    return yawRadians * (180.0 / SagaEngine::Math::kPiD);
}

double ViewportCamera::GetPitchDegrees() const noexcept
{
    return pitchRadians * (180.0 / SagaEngine::Math::kPiD);
}

// ─── Mutators ─────────────────────────────────────────────────────────────────

void ViewportCamera::Rotate(double deltaYawRadians,
                            double deltaPitchRadians) noexcept
{
    yawRadians   += deltaYawRadians;
    pitchRadians  = ClampPitch(pitchRadians + deltaPitchRadians);
}

void ViewportCamera::Zoom(float factor) noexcept
{
    if (!(factor > 0.0f))
    {
        return;
    }

    orbitRadius = std::clamp(orbitRadius * factor,
                             minOrbitRadius,
                             maxOrbitRadius);
}

void ViewportCamera::Pan(const Vec3& worldOffset) noexcept
{
    pivot = pivot + worldOffset;
}

void ViewportCamera::SetOrbitRadius(float radius) noexcept
{
    orbitRadius = std::clamp(radius, minOrbitRadius, maxOrbitRadius);
}

void ViewportCamera::SnapToAxis(AxisSnap axis) noexcept
{
    mode = ProjectionMode::Orthographic;

    // The (yaw, pitch) pairs below are derived from the forward
    // formula in GetForward() so that the snapped camera looks at
    // exactly the named world axis with up = +Y for horizontal views
    // and up = ±Z for top / bottom views.
    switch (axis)
    {
        case AxisSnap::Top:    yawRadians = 0.0;
                                pitchRadians = -kPitchLimitRadians; break;
        case AxisSnap::Bottom: yawRadians = 0.0;
                                pitchRadians =  kPitchLimitRadians; break;
        case AxisSnap::Front:  yawRadians = 0.0;
                                pitchRadians = 0.0;                 break;
        case AxisSnap::Back:   yawRadians = SagaEngine::Math::kPiD;
                                pitchRadians = 0.0;                 break;
        case AxisSnap::Right:  yawRadians =  SagaEngine::Math::kPiD / 2.0;
                                pitchRadians = 0.0;                 break;
        case AxisSnap::Left:   yawRadians = -SagaEngine::Math::kPiD / 2.0;
                                pitchRadians = 0.0;                 break;
    }
}

void ViewportCamera::FrameBounds(const Vec3& worldCentre,
                                 float       boundsRadius,
                                 float       aspect) noexcept
{
    const float radius = boundsRadius > 0.0f ? boundsRadius : 1.0f;

    pivot           = worldCentre;
    orthoHalfHeight = radius;

    if (mode == ProjectionMode::Perspective)
    {
        // Distance required for a vertical fov to enclose the sphere
        // exactly, plus a 25% margin so the sphere does not kiss the
        // viewport edge. A non-positive aspect falls back to 1.0
        // so square viewports do not blow up.
        const float halfFov = std::max(fovYRadians * 0.5f,
                                       kMinFovYRadians * 0.5f);
        const float distV = radius / std::tan(halfFov);

        // Account for narrow viewports — when aspect < 1, the
        // horizontal fov becomes the binding constraint.
        const float a     = aspect > 0.0f ? aspect : 1.0f;
        const float distH = a < 1.0f ? distV / a : distV;

        SetOrbitRadius(std::max(distV, distH) * 1.25f);
    }
    else
    {
        // Orthographic mode: frame the radius with a small margin and
        // pull the camera back enough that the near plane stays in
        // front of the framed bounds.
        orthoHalfHeight = radius * 1.1f;
        SetOrbitRadius(std::max(radius * 2.0f, minOrbitRadius));
    }
}

// ─── Validation ───────────────────────────────────────────────────────────────

bool ViewportCamera::Sanitize() noexcept
{
    bool changed = false;

    if (minOrbitRadius < 1.0e-4f)        { minOrbitRadius = 1.0e-4f;        changed = true; }
    if (maxOrbitRadius < minOrbitRadius) { maxOrbitRadius = minOrbitRadius; changed = true; }

    const float clamped = std::clamp(orbitRadius, minOrbitRadius, maxOrbitRadius);
    if (clamped != orbitRadius)          { orbitRadius   = clamped;         changed = true; }

    if (fovYRadians < kMinFovYRadians)   { fovYRadians   = kMinFovYRadians; changed = true; }
    if (fovYRadians > kMaxFovYRadians)   { fovYRadians   = kMaxFovYRadians; changed = true; }

    if (orthoHalfHeight <= 0.0f)         { orthoHalfHeight = 1.0f;          changed = true; }

    if (nearPlane < 1.0e-4f)             { nearPlane    = 1.0e-4f;          changed = true; }
    if (farPlane  < nearPlane + kMinClipSeparation)
    {
        farPlane = nearPlane + kMinClipSeparation;
        changed  = true;
    }

    const double pitchClamped = ClampPitch(pitchRadians);
    if (pitchClamped != pitchRadians)    { pitchRadians = pitchClamped;     changed = true; }

    return changed;
}

} // namespace SagaEditor
