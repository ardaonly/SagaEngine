/// @file CullingPipeline.h
/// @brief Walks RenderWorld for one camera; emits a RenderView draw list.
///
/// Layer  : SagaEngine / Render / Culling
/// Purpose: The culling pipeline is the bridge between "what exists"
///          (RenderWorld) and "what draws this frame for this camera"
///          (RenderView). It runs four cheap filters in order:
///            1. Hidden flag            (artist / server hide)
///            2. Visibility mask        (layer / group filter)
///            3. Distance               (max draw distance)
///            4. Frustum                (six-plane sphere test)
///          Occlusion culling is not here yet; it belongs after these
///          cheap tests once it lands and wants a GPU query budget.
///
/// Design rules:
///   - Single-threaded for the first iteration. The loop is trivially
///     parallelisable (per-chunk worklists) once the backend is running.
///   - Writes DrawItems with the selected LOD already filled in.
///   - Does NOT sort. Sorting is a separate concern the backend owns.

#pragma once

#include "SagaEngine/Render/LOD/LODSelector.h"
#include "SagaEngine/Render/Scene/Camera.h"
#include "SagaEngine/Render/Scene/RenderView.h"
#include "SagaEngine/Render/World/RenderEntity.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <cstdint>
#include <functional>

namespace SagaEngine::Render::Culling
{

namespace World = ::SagaEngine::Render::World;
namespace Scene = ::SagaEngine::Render::Scene;

// ─── LOD threshold source ─────────────────────────────────────────────

/// The render world stores MeshId handles, but LOD thresholds are a
/// property of the mesh asset (MeshLod::distanceThreshold). Rather than
/// pulling MeshAsset into the culler, the pipeline takes a callable that
/// maps MeshId → LodThresholds. The backend implements this against its
/// mesh resource table.
using LodThresholdLookupFn = std::function<
    ::SagaEngine::Render::LOD::LodThresholds(World::MeshId meshId)>;

// ─── Config ───────────────────────────────────────────────────────────

/// Tuning knobs used for one culling pass.
struct CullingConfig
{
    ::SagaEngine::Render::LOD::QualityPreset quality =
        ::SagaEngine::Render::LOD::QualityPreset::High;

    /// Reserve draw-item capacity up front to avoid reallocs. Picked
    /// from last frame's count by the caller; 0 skips reserve.
    std::size_t                              reserveHint = 0;
};

// ─── Pipeline ─────────────────────────────────────────────────────────

/// Culls a RenderWorld through a single camera into a RenderView.
/// Stateless beyond its configured LOD lookup hook; safe to reuse across
/// frames.
class CullingPipeline
{
public:
    CullingPipeline() = default;

    /// Install the mesh → thresholds lookup. If unset, every draw item
    /// resolves to LOD 0.
    void SetLodLookup(LodThresholdLookupFn fn) { m_LodLookup = std::move(fn); }

    /// Run the pipeline. Writes into `outView` after calling Reset() on it.
    void Run(const World::RenderWorld& world,
             const Scene::Camera&      camera,
             const CullingConfig&      config,
             Scene::RenderView&        outView) const;

private:
    LodThresholdLookupFn m_LodLookup;
};

} // namespace SagaEngine::Render::Culling
