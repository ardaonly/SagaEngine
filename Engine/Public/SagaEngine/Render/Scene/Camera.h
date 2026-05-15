/// @file Camera.h
/// @brief Render-time camera: position, view/projection, visibility filter.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: A Camera is the input to one render pass. It carries the data
///          the culling pipeline needs (view-proj matrix, world position,
///          visibility filter, LOD bias) plus a cached Frustum derived
///          from the view-proj matrix.
///
/// Design rules:
///   - Pure data. No GPU resources, no backend references.
///   - The view-projection matrix is the authoritative source; position
///     and frustum are derived. Callers build view-proj however they like
///     (perspective, orthographic, off-axis for shadow cascades) and then
///     call RecomputeFrustum() once.
///   - Visibility mask mirrors RenderEntity::visibilityMask — entities
///     are drawn only when (camera.filter & entity.mask) != 0.

#pragma once

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Math/Vec3.h"
#include "SagaEngine/Render/Scene/Frustum.h"
#include "SagaEngine/Render/World/RenderEntity.h"

#include <cmath>
#include <cstdint>

namespace SagaEngine::Render::Scene
{

// ─── Camera ───────────────────────────────────────────────────────────

/// Stable id for cameras registered with the render subsystem. The main
/// player camera, shadow cascades, minimap camera, etc. all live here.
enum class CameraId : std::uint32_t { kInvalid = 0xFFFFFFFFu };

/// Render-time camera description.
struct Camera
{
    ::SagaEngine::Math::Mat4 view       = ::SagaEngine::Math::Mat4::Identity();
    ::SagaEngine::Math::Mat4 projection = ::SagaEngine::Math::Mat4::Identity();
    ::SagaEngine::Math::Mat4 viewProj   = ::SagaEngine::Math::Mat4::Identity();

    /// World-space position used for distance culling / LOD selection.
    ::SagaEngine::Math::Vec3 position{0.0f, 0.0f, 0.0f};

    /// Cull anything beyond this distance from `position`. 0 disables.
    float                    maxDrawDistance = 0.0f;

    /// Multiplicative bias on LOD distance thresholds. >1 pushes LODs
    /// further out (higher quality), <1 pulls them closer (lower quality
    /// / faster). Typically set from a quality preset or user setting.
    float                    lodBias         = 1.0f;

    /// Only draw entities with `(entity.visibilityMask & filter) != 0`.
    ::SagaEngine::Render::World::VisibilityMask filter =
        ::SagaEngine::Render::World::kVisibilityAll;

    /// Frustum derived from view-proj. Call RecomputeFrustum() after
    /// mutating view / projection / viewProj.
    Frustum                  frustum{};

    // ── Derivation helpers ────────────────────────────────────────

    /// Multiply projection * view and cache the result in `viewProj`.
    /// Call this whenever `view` or `projection` change.
    void RecomputeViewProj() noexcept;

    /// Extract the six frustum planes from the current `viewProj`.
    /// Must be called after RecomputeViewProj(). Uses the standard
    /// Gribb / Hartmann method (row-combinations of the matrix).
    /// Planes are normalised so SignedDistance returns world-space units.
    void RecomputeFrustum() noexcept;
};

// ─── Inline implementation ────────────────────────────────────────────

inline void Camera::RecomputeViewProj() noexcept
{
    // projection * view, column-major (matches Mat4 convention).
    const ::SagaEngine::Math::Mat4& P = projection;
    const ::SagaEngine::Math::Mat4& V = view;
    ::SagaEngine::Math::Mat4 R{};
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += P.data[k * 4 + row] * V.data[col * 4 + k];
            R.data[col * 4 + row] = sum;
        }
    }
    viewProj = R;
}

inline void Camera::RecomputeFrustum() noexcept
{
    // Gribb/Hartmann plane extraction from a column-major view-proj.
    // m[col*4 + row].
    const auto& m = viewProj.data;

    auto setPlane = [&](Frustum::PlaneIndex idx, float a, float b, float c, float d)
    {
        const float len = std::sqrt(a * a + b * b + c * c);
        if (len <= 0.0f)
        {
            frustum.planes[idx] = {};
            return;
        }
        const float inv = 1.0f / len;
        frustum.planes[idx].normal = { a * inv, b * inv, c * inv };
        frustum.planes[idx].d      = d * inv;
    };

    // Row indexing: row r of viewProj = (m[0*4+r], m[1*4+r], m[2*4+r], m[3*4+r]).
    auto row = [&](int r, int col) { return m[col * 4 + r]; };

    // Left   =  row3 + row0
    setPlane(Frustum::Left,
             row(3, 0) + row(0, 0),
             row(3, 1) + row(0, 1),
             row(3, 2) + row(0, 2),
             row(3, 3) + row(0, 3));

    // Right  =  row3 - row0
    setPlane(Frustum::Right,
             row(3, 0) - row(0, 0),
             row(3, 1) - row(0, 1),
             row(3, 2) - row(0, 2),
             row(3, 3) - row(0, 3));

    // Bottom =  row3 + row1
    setPlane(Frustum::Bottom,
             row(3, 0) + row(1, 0),
             row(3, 1) + row(1, 1),
             row(3, 2) + row(1, 2),
             row(3, 3) + row(1, 3));

    // Top    =  row3 - row1
    setPlane(Frustum::Top,
             row(3, 0) - row(1, 0),
             row(3, 1) - row(1, 1),
             row(3, 2) - row(1, 2),
             row(3, 3) - row(1, 3));

    // Near   =  row3 + row2 (works for both D3D [0..1] and GL [-1..1] clip
    // spaces when the projection is constructed consistently with the
    // engine's convention — right-handed, zero-to-one or negative-one-to-
    // one depth both drop the same plane equation after normalisation.)
    setPlane(Frustum::Near,
             row(3, 0) + row(2, 0),
             row(3, 1) + row(2, 1),
             row(3, 2) + row(2, 2),
             row(3, 3) + row(2, 3));

    // Far    =  row3 - row2
    setPlane(Frustum::Far,
             row(3, 0) - row(2, 0),
             row(3, 1) - row(2, 1),
             row(3, 2) - row(2, 2),
             row(3, 3) - row(2, 3));
}

} // namespace SagaEngine::Render::Scene
