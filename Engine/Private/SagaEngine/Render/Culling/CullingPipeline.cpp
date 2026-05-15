/// @file CullingPipeline.cpp
/// @brief Walks RenderWorld, applies hidden/mask/distance/frustum gates,
///        selects LOD, and appends draw items to the RenderView.

#include "SagaEngine/Render/Culling/CullingPipeline.h"

namespace SagaEngine::Render::Culling
{

namespace
{

[[nodiscard]] inline float DistanceSq(const ::SagaEngine::Math::Vec3& a,
                                        const ::SagaEngine::Math::Vec3& b) noexcept
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

// ─── CullingPipeline::Run ─────────────────────────────────────────────

void CullingPipeline::Run(const World::RenderWorld& world,
                            const Scene::Camera&      camera,
                            const CullingConfig&      config,
                            Scene::RenderView&        outView) const
{
    outView.Reset();

    if (config.reserveHint > 0)
        outView.drawItems.reserve(config.reserveHint);

    const float maxDistSq = camera.maxDrawDistance * camera.maxDrawDistance;
    const bool  hasDistLimit = camera.maxDrawDistance > 0.0f;

    const auto& entities = world.EntitiesView();
    const auto& alive    = world.AliveSlotsView();

    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        if (!alive[i]) continue;

        const World::RenderEntity& e = entities[i];
        ++outView.visited;

        // 1. Hidden flag.
        if (!e.visible)
        {
            ++outView.culledHidden;
            continue;
        }

        // 2. Visibility mask — empty intersection means this camera
        //    does not render this layer.
        if ((e.visibilityMask & camera.filter) == 0u)
        {
            ++outView.culledVisibility;
            continue;
        }

        // 3. Distance.
        const float distSq = DistanceSq(camera.position, e.transform.position);
        if (hasDistLimit && distSq > maxDistSq)
        {
            ++outView.culledDistance;
            continue;
        }

        // 4. Frustum (conservative sphere test).
        if (!camera.frustum.IntersectsSphere(e.transform.position, e.boundsRadius))
        {
            ++outView.culledFrustum;
            continue;
        }

        // ── Survivor: pick LOD + emit draw item. ─────────────────
        std::uint8_t lod = 0;
        if (e.lodOverride != World::kLodOverrideAuto)
        {
            lod = e.lodOverride;
        }
        else if (m_LodLookup)
        {
            const auto thresholds = m_LodLookup(e.mesh);
            lod = ::SagaEngine::Render::LOD::SelectLodSq(
                thresholds, distSq, config.quality, camera.lodBias);
        }

        Scene::DrawItem item;
        item.entity     = static_cast<World::RenderEntityId>(i);
        item.mesh       = e.mesh;
        item.material   = e.material;
        item.lod        = lod;
        item.distanceSq = distSq;
        outView.drawItems.push_back(item);
    }
}

} // namespace SagaEngine::Render::Culling
