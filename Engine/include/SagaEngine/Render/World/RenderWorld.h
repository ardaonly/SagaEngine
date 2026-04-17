/// @file RenderWorld.h
/// @brief Client-side container of every drawable the renderer can see.
///
/// Layer  : SagaEngine / Render / World
/// Purpose: The render world is what the culling + LOD pipeline walks. It
///          owns the RenderEntity pool, issues stable handles, and exposes
///          the bare minimum of mutation / iteration surface the pipeline
///          needs. Gameplay, replication, and the network input layer do
///          NOT touch this class directly — they go through a bridge
///          (NetEntityBinding) which owns NetworkId → RenderEntityId maps
///          and updates transforms / mesh swaps / visibility flips.
///
/// Design rules:
///   - Dense storage. Entities live in a std::vector<RenderEntity>; the
///     RenderEntityId carries the vector index. Freed slots are kept on a
///     free-list and re-used, but the *handle* also carries a generation
///     counter (future extension — not needed for the first iteration).
///   - Stable ids during a single frame. The pipeline is free to cache a
///     RenderEntityId inside a draw list; Destroy() runs at frame
///     boundaries, not mid-cull.
///   - Single-threaded mutation; parallel reads. Add/Remove/SetTransform
///     are called on one thread (typically the game thread). Culling can
///     read the pool from worker threads once the frame is frozen.

#pragma once

#include "SagaEngine/Render/World/RenderEntity.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace SagaEngine::Render::World
{

// ─── RenderWorld ──────────────────────────────────────────────────────

/// Dense pool of RenderEntity records plus a free-list for reuse.
///
/// The class intentionally does not know about cameras, culling, LOD, or
/// GPU resources. Those concerns live in higher layers that read this
/// class as a data source.
class RenderWorld
{
public:
    RenderWorld()  = default;
    ~RenderWorld() = default;

    RenderWorld(const RenderWorld&)            = delete;
    RenderWorld& operator=(const RenderWorld&) = delete;

    /// Allocate a new entity slot and copy `initial` into it. The returned
    /// handle is stable until Destroy() is called on it.
    RenderEntityId Create(const RenderEntity& initial);

    /// Release an entity slot. The handle becomes invalid immediately;
    /// the underlying slot may be reused by a later Create().
    void Destroy(RenderEntityId id);

    /// Does `id` point to a live entity right now?
    [[nodiscard]] bool IsAlive(RenderEntityId id) const noexcept;

    /// Const / mutable access by handle. Nullptr if `id` is stale.
    [[nodiscard]] const RenderEntity* Get(RenderEntityId id) const noexcept;
    [[nodiscard]] RenderEntity*       Get(RenderEntityId id)       noexcept;

    // ── Common single-field mutators used by the bridge ───────────

    bool SetTransform(RenderEntityId id,
                      const ::SagaEngine::Math::Transform& transform) noexcept;
    bool SetMesh     (RenderEntityId id, MeshId     mesh)     noexcept;
    bool SetMaterial (RenderEntityId id, MaterialId material) noexcept;
    bool SetVisible  (RenderEntityId id, bool visible)        noexcept;
    bool SetVisibilityMask(RenderEntityId id, VisibilityMask mask) noexcept;
    bool SetLodOverride   (RenderEntityId id, std::uint8_t lod)    noexcept;
    bool SetBoundsRadius  (RenderEntityId id, float radius)        noexcept;

    // ── Bulk query surface for culling ────────────────────────────

    /// Number of entity slots (alive or free). Use IsAlive to filter.
    [[nodiscard]] std::size_t Capacity() const noexcept { return m_Entities.size(); }

    /// Number of live entities.
    [[nodiscard]] std::size_t LiveCount() const noexcept { return m_LiveCount; }

    /// Direct access to the underlying slot array. Indices correspond to
    /// RenderEntityId values. Alive-state must be checked with the
    /// parallel IsAlive vector (AliveSlotsView) — the pool is dense, so
    /// holes exist after destroys.
    [[nodiscard]] const std::vector<RenderEntity>& EntitiesView() const noexcept { return m_Entities; }
    [[nodiscard]] const std::vector<std::uint8_t>& AliveSlotsView() const noexcept { return m_Alive; }

    /// Call `fn(id, entity)` for every live entity. Iteration order is
    /// slot-index (cache friendly). Callback must not mutate the world.
    template <typename Fn>
    void ForEach(Fn&& fn) const
    {
        for (std::size_t i = 0; i < m_Entities.size(); ++i)
        {
            if (!m_Alive[i]) continue;
            fn(static_cast<RenderEntityId>(i), m_Entities[i]);
        }
    }

private:
    std::vector<RenderEntity>     m_Entities;   ///< Dense pool.
    std::vector<std::uint8_t>     m_Alive;      ///< Parallel alive flags (0/1).
    std::vector<std::uint32_t>    m_FreeList;   ///< Reusable slot indices.
    std::size_t                   m_LiveCount = 0;
};

} // namespace SagaEngine::Render::World
