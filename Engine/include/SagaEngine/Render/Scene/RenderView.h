/// @file RenderView.h
/// @brief Per-camera cull + LOD output consumed by the backend submission layer.
///
/// Layer  : SagaEngine / Render / Scene
/// Purpose: One RenderView per camera per frame. The culling pipeline writes
///          into it; the backend (Diligent, later) reads it to produce GPU
///          draw calls. This is the backend-agnostic draw list.
///
/// Design rules:
///   - One draw item per visible (entity, LOD) pair.
///   - The view owns no GPU state. Mesh / material handles carried here
///     are resolved by the backend at submit time.
///   - Items are not sorted by the culler; a separate sort pass (by
///     material, depth, or whatever the backend prefers) runs before
///     submission. Keeping that out of the view lets different backends
///     pick different sort strategies without changing the cull code.

#pragma once

#include "SagaEngine/Math/Mat4.h"
#include "SagaEngine/Render/World/RenderEntity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SagaEngine::Render::Scene
{

// ─── DrawItem ─────────────────────────────────────────────────────────

/// One draw command at the backend-agnostic level.
struct DrawItem
{
    ::SagaEngine::Render::World::RenderEntityId entity =
        ::SagaEngine::Render::World::RenderEntityId::kInvalid;

    ::SagaEngine::Render::World::MeshId     mesh     =
        ::SagaEngine::Render::World::MeshId::kInvalid;

    ::SagaEngine::Render::World::MaterialId material =
        ::SagaEngine::Render::World::MaterialId::kInvalid;

    /// Object-to-world transform. Backend uploads this to the per-draw
    /// constant buffer so the shader can transform vertices and normals.
    ::SagaEngine::Math::Mat4 model = ::SagaEngine::Math::Mat4::Identity();

    std::uint8_t lod = 0;   ///< Selected LOD level (0 = highest detail).

    /// Squared distance from camera to entity position. Kept as squared
    /// distance so sort passes can compare without a sqrt. Populated by
    /// the culling pipeline so the LOD pass does not recompute it.
    float        distanceSq = 0.0f;
};

// ─── RenderView ───────────────────────────────────────────────────────

/// The draw list + per-view statistics for a single camera.
struct RenderView
{
    std::vector<DrawItem> drawItems;

    // ── Per-frame counters (diagnostic) ──────────────────────────

    std::uint32_t visited         = 0;   ///< Entities the culler walked.
    std::uint32_t culledFrustum   = 0;   ///< Rejected by frustum test.
    std::uint32_t culledDistance  = 0;   ///< Rejected by max-draw-distance.
    std::uint32_t culledVisibility= 0;   ///< Rejected by visibility mask.
    std::uint32_t culledHidden    = 0;   ///< RenderEntity::visible == false.

    /// Clear draw items and counters. Call once per frame before culling.
    void Reset()
    {
        drawItems.clear();
        visited          = 0;
        culledFrustum    = 0;
        culledDistance   = 0;
        culledVisibility = 0;
        culledHidden     = 0;
    }

    [[nodiscard]] std::size_t DrawCount() const noexcept { return drawItems.size(); }
};

} // namespace SagaEngine::Render::Scene
