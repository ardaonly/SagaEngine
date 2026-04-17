/// @file FrustumTests.cpp
/// @brief Frustum plane extraction and intersection tests.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: The Gribb/Hartmann plane extraction and the sphere / AABB
///          tests are the CPU culling hot path. A subtle sign flip here
///          would silently drop every entity from view; these tests lock
///          behaviour down with hand-built matrices whose frustum shape
///          is easy to reason about.

#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/Frustum.h"

#include <gtest/gtest.h>

using SagaEngine::Render::Scene::Camera;
using SagaEngine::Render::Scene::Frustum;
using SagaEngine::Render::Scene::Plane;
using SagaEngine::Math::Mat4;
using SagaEngine::Math::Vec3;

// ─── Frustum primitive tests (hand-built unit cube) ───────────────────

namespace
{
/// Build the frustum corresponding to the clip-space unit cube
/// [-1, 1]^3. Planes have inward-facing normals: each plane passes
/// through ±1 on its axis.
Frustum UnitCubeFrustum() noexcept
{
    Frustum f{};
    // Left   x >= -1 : n = (+1, 0, 0), d = +1  (n·x + d >= 0 when x >= -1)
    f.planes[Frustum::Left  ] = {Vec3{ 1, 0, 0}, 1.0f};
    // Right  x <= +1 : n = (-1, 0, 0), d = +1
    f.planes[Frustum::Right ] = {Vec3{-1, 0, 0}, 1.0f};
    // Bottom y >= -1
    f.planes[Frustum::Bottom] = {Vec3{ 0, 1, 0}, 1.0f};
    // Top    y <= +1
    f.planes[Frustum::Top   ] = {Vec3{ 0,-1, 0}, 1.0f};
    // Near   z >= -1
    f.planes[Frustum::Near  ] = {Vec3{ 0, 0, 1}, 1.0f};
    // Far    z <= +1
    f.planes[Frustum::Far   ] = {Vec3{ 0, 0,-1}, 1.0f};
    return f;
}
} // namespace

TEST(Frustum, SphereAtOriginIsInside)
{
    const auto f = UnitCubeFrustum();
    EXPECT_TRUE(f.IntersectsSphere(Vec3{0, 0, 0}, 0.1f));
}

TEST(Frustum, SphereWellOutsideIsRejected)
{
    const auto f = UnitCubeFrustum();
    EXPECT_FALSE(f.IntersectsSphere(Vec3{5, 0, 0}, 0.1f));
    EXPECT_FALSE(f.IntersectsSphere(Vec3{0, -5, 0}, 0.4f));
}

TEST(Frustum, SphereStraddlingPlaneIsConservativelyInside)
{
    // Centre at x=1.5 is 0.5 outside the right plane (x<=1). Sphere with
    // radius 0.6 crosses the plane and must be treated as inside.
    const auto f = UnitCubeFrustum();
    EXPECT_TRUE(f.IntersectsSphere(Vec3{1.5f, 0, 0}, 0.6f));
    // Radius only 0.3 → fully outside, rejected.
    EXPECT_FALSE(f.IntersectsSphere(Vec3{1.5f, 0, 0}, 0.3f));
}

TEST(Frustum, SphereExactlyOnPlaneCountsAsInside)
{
    const auto f = UnitCubeFrustum();
    // Signed distance is zero on the plane — conservative test keeps it.
    EXPECT_TRUE(f.IntersectsSphere(Vec3{1.0f, 0, 0}, 0.0f));
}

TEST(Frustum, AabbInsideAndOutside)
{
    const auto f = UnitCubeFrustum();
    // Fully inside.
    EXPECT_TRUE(f.IntersectsAabb(Vec3{-0.5f, -0.5f, -0.5f},
                                   Vec3{ 0.5f,  0.5f,  0.5f}));
    // Fully outside (entirely past +X).
    EXPECT_FALSE(f.IntersectsAabb(Vec3{2.0f, -0.5f, -0.5f},
                                    Vec3{3.0f,  0.5f,  0.5f}));
    // Overlap on +X — positive-vertex test keeps it.
    EXPECT_TRUE(f.IntersectsAabb(Vec3{0.5f, -0.5f, -0.5f},
                                   Vec3{2.0f,  0.5f,  0.5f}));
}

// ─── Camera plane extraction ──────────────────────────────────────────

TEST(CameraFrustum, IdentityViewProjExtractsUnitCube)
{
    Camera cam{};
    cam.view       = Mat4::Identity();
    cam.projection = Mat4::Identity();
    cam.RecomputeViewProj();
    cam.RecomputeFrustum();

    // Origin is inside.
    EXPECT_TRUE(cam.frustum.IntersectsSphere(Vec3{0, 0, 0}, 0.01f));
    // Points just outside each face are rejected when the sphere is tiny.
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{ 1.2f, 0, 0}, 0.05f));
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{-1.2f, 0, 0}, 0.05f));
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{0,  1.2f, 0}, 0.05f));
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{0, -1.2f, 0}, 0.05f));
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{0, 0,  1.2f}, 0.05f));
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{0, 0, -1.2f}, 0.05f));
}

TEST(CameraFrustum, RecomputeViewProjMultipliesPtimesV)
{
    Camera cam{};
    cam.view       = Mat4::Identity();
    cam.projection = Mat4::Identity();
    cam.RecomputeViewProj();
    for (int i = 0; i < 16; ++i)
    {
        const float expected = (i % 5 == 0) ? 1.0f : 0.0f;  // identity pattern
        EXPECT_FLOAT_EQ(cam.viewProj.data[i], expected);
    }
}

TEST(CameraFrustum, FrustumFollowsViewProjMutations)
{
    Camera cam{};
    cam.view       = Mat4::Identity();
    cam.projection = Mat4::Identity();
    cam.RecomputeViewProj();
    cam.RecomputeFrustum();
    EXPECT_TRUE(cam.frustum.IntersectsSphere(Vec3{0, 0, 0}, 0.01f));

    // Scale the projection so the frustum shrinks to [-0.5, 0.5]^3.
    // A "scale" matrix with diag (2,2,2,1) halves clip-space coverage.
    Mat4 scale = Mat4::Identity();
    scale.data[0]  = 2.0f;   // m00
    scale.data[5]  = 2.0f;   // m11
    scale.data[10] = 2.0f;   // m22
    cam.projection = scale;
    cam.RecomputeViewProj();
    cam.RecomputeFrustum();

    // A point that was inside the unit cube frustum (x=0.8) is now outside.
    EXPECT_FALSE(cam.frustum.IntersectsSphere(Vec3{0.8f, 0, 0}, 0.05f));
    // A point deeper inside the new frustum is still inside.
    EXPECT_TRUE (cam.frustum.IntersectsSphere(Vec3{0.1f, 0, 0}, 0.05f));
}
