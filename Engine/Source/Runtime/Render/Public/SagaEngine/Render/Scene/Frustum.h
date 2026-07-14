/// @file Frustum.h
/// @brief Six-plane view frustum with sphere / AABB intersection tests.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: The frustum culler walks RenderWorld once per camera and asks
///          "is this entity's bounding sphere at least partially inside my
///          view volume?". Frustum stores the six camera planes in world
///          space with outward-facing normals; a point is inside when its
///          signed distance to every plane is >= 0.
///
/// Design rules:
///   - API-agnostic. No Diligent / D3D / Vulkan includes. Planes are raw
///     floats plus SagaEngine::Math::Vec3.
///   - Extraction from a view-projection matrix lives in Camera.h — this
///     header defines only the frustum *type* and its tests.
///   - Tests use simple "conservative": a sphere that straddles a plane
///     is counted as inside. This keeps culling fast; any false-positive
///     is caught by GPU-side depth rejection later anyway.

#pragma once

#include "SagaEngine/Math/Vec3.h"

#include <array>
#include <cstdint>

namespace SagaEngine::Render::Scene
{

// ─── Plane ────────────────────────────────────────────────────────────

/// World-space plane in point-normal form: n·x + d == 0. `normal` points
/// INTO the frustum (so points inside have non-negative signed distance).
struct Plane
{
    ::SagaEngine::Math::Vec3 normal{0.0f, 0.0f, 0.0f};
    float                    d = 0.0f;

    [[nodiscard]] constexpr float SignedDistance(
        const ::SagaEngine::Math::Vec3& p) const noexcept
    {
        return normal.x * p.x + normal.y * p.y + normal.z * p.z + d;
    }
};

// ─── Frustum ──────────────────────────────────────────────────────────

/// Six-plane view frustum (left, right, bottom, top, near, far).
/// Planes are world-space with normals facing inward.
struct Frustum
{
    enum PlaneIndex : std::uint8_t
    {
        Left   = 0,
        Right  = 1,
        Bottom = 2,
        Top    = 3,
        Near   = 4,
        Far    = 5,
        Count  = 6,
    };

    std::array<Plane, Count> planes{};

    // ── Tests ────────────────────────────────────────────────────

    /// Test a world-space sphere against all six planes. Returns true if
    /// the sphere is at least partially inside the frustum. Conservative:
    /// a sphere whose centre is outside but whose radius crosses the
    /// plane is still considered inside.
    [[nodiscard]] bool IntersectsSphere(const ::SagaEngine::Math::Vec3& centre,
                                          float radius) const noexcept
    {
        for (const auto& p : planes)
        {
            if (p.SignedDistance(centre) < -radius)
                return false;
        }
        return true;
    }

    /// Test a world-space AABB given by min/max corners. Uses the
    /// standard "positive-vertex" trick: for each plane, evaluate only
    /// the farthest corner in the plane's normal direction.
    [[nodiscard]] bool IntersectsAabb(const ::SagaEngine::Math::Vec3& mn,
                                        const ::SagaEngine::Math::Vec3& mx) const noexcept
    {
        for (const auto& p : planes)
        {
            const ::SagaEngine::Math::Vec3 positive{
                p.normal.x >= 0.0f ? mx.x : mn.x,
                p.normal.y >= 0.0f ? mx.y : mn.y,
                p.normal.z >= 0.0f ? mx.z : mn.z,
            };
            if (p.SignedDistance(positive) < 0.0f)
                return false;
        }
        return true;
    }
};

} // namespace SagaEngine::Render::Scene
