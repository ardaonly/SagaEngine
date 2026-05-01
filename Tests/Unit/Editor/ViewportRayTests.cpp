/// @file ViewportRayTests.cpp
/// @brief GoogleTest coverage for `ViewportRay` and gizmo handle hit-testing.

#include "SagaEditor/Viewport/GizmoHandle.h"
#include "SagaEditor/Viewport/ViewportCamera.h"
#include "SagaEditor/Viewport/ViewportRay.h"

#include "SagaEngine/Math/MathConstants.h"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace
{

using namespace SagaEditor;
using SagaEngine::Math::Vec3;

constexpr float kEps = 1.0e-3f;

[[nodiscard]] bool VecApprox(const Vec3& a, const Vec3& b, float eps = kEps) noexcept
{
    return std::fabs(a.x - b.x) < eps &&
           std::fabs(a.y - b.y) < eps &&
           std::fabs(a.z - b.z) < eps;
}

// ─── ViewportRay ──────────────────────────────────────────────────────────────

TEST(ViewportRayTest, PointAtScalesDirection)
{
    ViewportRay r;
    r.origin    = Vec3{1.0f, 2.0f, 3.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};

    EXPECT_TRUE(VecApprox(r.PointAt(0.0f), Vec3{1.0f, 2.0f,  3.0f}));
    EXPECT_TRUE(VecApprox(r.PointAt(2.0f), Vec3{1.0f, 2.0f,  1.0f}));
}

TEST(ViewportRayTest, ScreenCenterPointsThroughCameraForward)
{
    ViewportCamera cam;
    cam.pivot       = Vec3{0.0f, 0.0f, 0.0f};
    cam.orbitRadius = 5.0f;

    const auto ray = ScreenToWorldRay(cam, 640.0f, 360.0f, 1280.0f, 720.0f);
    // Camera forward = -Z by default, ray direction must point along it.
    EXPECT_TRUE(VecApprox(ray.direction, cam.GetForward()));
    EXPECT_TRUE(VecApprox(ray.origin,    cam.GetPosition()));
}

TEST(ViewportRayTest, ScreenEdgesProduceDifferentDirections)
{
    ViewportCamera cam;
    const auto centre = ScreenToWorldRay(cam, 640.0f, 360.0f, 1280.0f, 720.0f);
    const auto left   = ScreenToWorldRay(cam,   0.0f, 360.0f, 1280.0f, 720.0f);
    const auto right  = ScreenToWorldRay(cam,1280.0f, 360.0f, 1280.0f, 720.0f);

    EXPECT_FALSE(VecApprox(centre.direction, left.direction));
    EXPECT_FALSE(VecApprox(centre.direction, right.direction));
    // Left and right rays are mirror images across the camera forward.
    EXPECT_NEAR(left.direction.x, -right.direction.x, kEps);
}

TEST(ViewportRayTest, DegenerateViewportProducesForwardRay)
{
    ViewportCamera cam;
    const auto ray = ScreenToWorldRay(cam, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(VecApprox(ray.direction, cam.GetForward()));
    EXPECT_TRUE(VecApprox(ray.origin,    cam.GetPosition()));
}

TEST(ViewportRayTest, OrthoModeRayDirectionEqualsCameraForward)
{
    ViewportCamera cam;
    cam.SnapToAxis(AxisSnap::Front); // mode = Orthographic, forward = -Z
    const auto ray = ScreenToWorldRay(cam, 100.0f, 100.0f, 1000.0f, 1000.0f);
    EXPECT_TRUE(VecApprox(ray.direction, Vec3{0.0f, 0.0f, -1.0f}, 5.0e-3f));
}

// ─── Plane Intersection ───────────────────────────────────────────────────────

TEST(ViewportRayTest, RayHitsHorizontalGroundPlane)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 5.0f, 0.0f};
    r.direction = Vec3{0.0f, -1.0f, 0.0f};

    const auto t = IntersectRayPlane(r, Vec3{0.0f, 0.0f, 0.0f},
                                        Vec3{0.0f, 1.0f, 0.0f});
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 5.0f, kEps);
}

TEST(ViewportRayTest, RayParallelToPlaneReturnsNullopt)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 5.0f, 0.0f};
    r.direction = Vec3{1.0f, 0.0f, 0.0f};
    EXPECT_FALSE(IntersectRayPlane(r, Vec3{}, Vec3{0.0f, 1.0f, 0.0f}).has_value());
}

TEST(ViewportRayTest, RayBehindPlaneReturnsNegativeT)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, -5.0f, 0.0f};
    r.direction = Vec3{0.0f, -1.0f, 0.0f};
    const auto t = IntersectRayPlane(r, Vec3{}, Vec3{0.0f, 1.0f, 0.0f});
    ASSERT_TRUE(t.has_value());
    EXPECT_LT(*t, 0.0f);
}

// ─── Sphere Intersection ──────────────────────────────────────────────────────

TEST(ViewportRayTest, RayHitsSphereInFront)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 0.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    const auto t = IntersectRaySphere(r, Vec3{0.0f, 0.0f, 0.0f}, 1.0f);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 4.0f, kEps);
}

TEST(ViewportRayTest, RayMissesSphere)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 5.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    EXPECT_FALSE(IntersectRaySphere(r, Vec3{0.0f, 0.0f, 0.0f}, 1.0f).has_value());
}

TEST(ViewportRayTest, RayInsideSphereReturnsFarHit)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 0.0f, 0.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    const auto t = IntersectRaySphere(r, Vec3{0.0f, 0.0f, 0.0f}, 2.0f);
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(*t, 2.0f, kEps);
}

TEST(ViewportRayTest, NonPositiveRadiusReturnsNullopt)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 0.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    EXPECT_FALSE(IntersectRaySphere(r, Vec3{0.0f, 0.0f, 0.0f},  0.0f).has_value());
    EXPECT_FALSE(IntersectRaySphere(r, Vec3{0.0f, 0.0f, 0.0f}, -1.0f).has_value());
}

// ─── Ray ↔ Segment ────────────────────────────────────────────────────────────

TEST(ViewportRayTest, ClosestRayToSegmentParallelDirectionRejected)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 1.0f, 0.0f};
    r.direction = Vec3{1.0f, 0.0f, 0.0f};
    EXPECT_FALSE(ClosestRayToSegment(r, Vec3{0.0f, 0.0f, 0.0f},
                                         Vec3{5.0f, 0.0f, 0.0f}).has_value());
}

TEST(ViewportRayTest, ClosestRayToSegmentMeasuresPerpendicularDistance)
{
    ViewportRay r;
    r.origin    = Vec3{0.0f, 1.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};

    const auto closest = ClosestRayToSegment(r, Vec3{-5.0f, 0.0f, 0.0f},
                                                Vec3{ 5.0f, 0.0f, 0.0f});
    ASSERT_TRUE(closest.has_value());
    // Ray runs through (0, 1, 0) when t=5; segment passes (0,0,0) at s=0.5.
    EXPECT_NEAR(closest->s, 0.5f, kEps);
    EXPECT_NEAR(closest->distSq, 1.0f, kEps);
}

TEST(ViewportRayTest, ClosestRayToSegmentClampsBeyondEndpoint)
{
    ViewportRay r;
    r.origin    = Vec3{10.0f, 1.0f,  5.0f};
    r.direction = Vec3{ 0.0f, 0.0f, -1.0f};

    const auto closest = ClosestRayToSegment(r, Vec3{-5.0f, 0.0f, 0.0f},
                                                Vec3{ 5.0f, 0.0f, 0.0f});
    ASSERT_TRUE(closest.has_value());
    // Closest segment point is the +X endpoint (s = 1).
    EXPECT_NEAR(closest->s, 1.0f, kEps);
}

// ─── Gizmo Hit-Test ───────────────────────────────────────────────────────────

TEST(GizmoHandleTest, IdsAreNonEmptyAndUnique)
{
    const GizmoHandle all[] = {
        GizmoHandle::AxisX, GizmoHandle::AxisY, GizmoHandle::AxisZ,
        GizmoHandle::PlaneXY, GizmoHandle::PlaneYZ, GizmoHandle::PlaneXZ,
        GizmoHandle::RotationX, GizmoHandle::RotationY, GizmoHandle::RotationZ,
        GizmoHandle::ScaleUniform,
    };
    EXPECT_EQ(std::string(GizmoHandleId(GizmoHandle::None)), "");

    std::string seen[std::size(all)];
    for (size_t i = 0; i < std::size(all); ++i)
    {
        seen[i] = GizmoHandleId(all[i]);
        EXPECT_FALSE(seen[i].empty());
        for (size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(seen[i], seen[j]);
        }
    }
}

TEST(GizmoHandleTest, TranslateGizmoXAxisHit)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.05f;

    ViewportRay r;
    r.origin    = Vec3{0.5f, 0.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};

    const auto hit = HitTestTranslateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::AxisX);
    EXPECT_GT(hit->rayT, 0.0f);
}

TEST(GizmoHandleTest, TranslateGizmoYAxisHit)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.05f;

    ViewportRay r;
    r.origin    = Vec3{0.0f, 0.5f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};

    const auto hit = HitTestTranslateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::AxisY);
}

TEST(GizmoHandleTest, TranslateGizmoMissReturnsNullopt)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.02f;
    cfg.planeHalfExtent  = 0.0f;
    cfg.planeHitTolerance = 0.0f;

    ViewportRay r;
    r.origin    = Vec3{ 5.0f, 5.0f,  5.0f};
    r.direction = Vec3{ 0.0f, 0.0f, -1.0f};
    EXPECT_FALSE(HitTestTranslateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg).has_value());
}

TEST(GizmoHandleTest, TranslateGizmoBehindCameraRejected)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.05f;
    cfg.planeHalfExtent  = 0.0f;
    cfg.planeHitTolerance = 0.0f;

    ViewportRay r;
    r.origin    = Vec3{0.5f, 0.0f, -5.0f}; // behind the gizmo on -Z
    r.direction = Vec3{0.0f, 0.0f, -1.0f}; // walking even further away
    EXPECT_FALSE(HitTestTranslateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg).has_value());
}

TEST(GizmoHandleTest, TranslateGizmoPlaneXYHit)
{
    GizmoHitConfig cfg;
    cfg.axisLength        = 1.0f;
    cfg.planeHalfExtent   = 0.25f;
    cfg.axisHitTolerance  = 0.01f;
    cfg.planeHitTolerance = 0.01f;

    // Aim a ray straight down +Z direction's negative through the
    // middle of the XY plane handle (which spans X in [0, 0.5], Y in
    // [0, 0.5]).
    ViewportRay r;
    r.origin    = Vec3{0.25f, 0.25f,  5.0f};
    r.direction = Vec3{0.0f,  0.0f,  -1.0f};

    const auto hit = HitTestTranslateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::PlaneXY);
}

TEST(GizmoHandleTest, RotateGizmoZRingHit)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.05f;

    // Ray running down -Z, crossing the Z=0 plane at radius 1 from
    // the gizmo origin → rotation-Z ring hit.
    ViewportRay r;
    r.origin    = Vec3{ 1.0f, 0.0f,  5.0f};
    r.direction = Vec3{ 0.0f, 0.0f, -1.0f};
    const auto hit = HitTestRotateGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::RotationZ);
}

TEST(GizmoHandleTest, ScaleGizmoUniformCentreHit)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.10f;

    // Ray pointed straight at the gizmo origin from along +Z hits
    // the uniform-scale sphere first.
    ViewportRay r;
    r.origin    = Vec3{0.0f, 0.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    const auto hit = HitTestScaleGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::ScaleUniform);
}

TEST(GizmoHandleTest, ScaleGizmoFallsBackToAxisOnGrazingMiss)
{
    GizmoHitConfig cfg;
    cfg.axisLength       = 1.0f;
    cfg.axisHitTolerance = 0.05f;

    // A ray offset along +X cannot hit the uniform sphere (radius
    // ~0.075) but lies on the +X axis arrow.
    ViewportRay r;
    r.origin    = Vec3{0.5f, 0.0f,  5.0f};
    r.direction = Vec3{0.0f, 0.0f, -1.0f};
    const auto hit = HitTestScaleGizmo(r, Vec3{0.0f, 0.0f, 0.0f}, cfg);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->handle, GizmoHandle::AxisX);
}

} // namespace
