/// @file NetEntityBinding.h
/// @brief Maps replicated network entities to RenderWorld entities.
///
/// Layer  : SagaEngine / Render / Bridge
/// Purpose: Replication hands the client "this network id is at this
///          transform, wearing this mesh, visible=true". The bridge turns
///          that stream of updates into Create / SetTransform / Destroy
///          calls on the RenderWorld. Gameplay and render worlds stay
///          decoupled: a network stall leaves the last-known-good render
///          entity exactly where it was, interpolation upstream decides
///          how to smooth it.
///
/// Design rules:
///   - One-way: network → render. Gameplay never reads render state back
///     through this class.
///   - Purely client-side. Never compiled into a dedicated server build.
///   - Does not own the RenderWorld — takes a reference.

#pragma once

#include "SagaEngine/Render/World/RenderEntity.h"
#include "SagaEngine/Render/World/RenderWorld.h"

#include <cstdint>
#include <unordered_map>

namespace SagaEngine::Render::Bridge
{

namespace World = ::SagaEngine::Render::World;

// ─── NetEntityBinding ─────────────────────────────────────────────────

/// Translates replicated state updates into RenderWorld mutations.
class NetEntityBinding
{
public:
    explicit NetEntityBinding(World::RenderWorld& world) noexcept
        : m_World(&world) {}

    /// Register / update a render entity for `networkId`. If no mapping
    /// exists yet one is created; otherwise the initial record is used
    /// to update the existing render entity.
    World::RenderEntityId BindOrCreate(std::uint32_t networkId,
                                         const World::RenderEntity& initial);

    /// Remove the mapping and destroy the render entity.
    /// Safe to call for unknown ids (no-op).
    void Release(std::uint32_t networkId);

    /// Resolve a network id to its render handle.
    /// Returns RenderEntityId::kInvalid if unknown.
    [[nodiscard]] World::RenderEntityId Resolve(std::uint32_t networkId) const noexcept;

    // ── Hot-path update accessors ─────────────────────────────────

    bool UpdateTransform(std::uint32_t networkId,
                          const ::SagaEngine::Math::Transform& transform) noexcept;
    bool UpdateVisible  (std::uint32_t networkId, bool visible) noexcept;
    bool UpdateMesh     (std::uint32_t networkId, World::MeshId mesh) noexcept;
    bool UpdateMaterial (std::uint32_t networkId, World::MaterialId material) noexcept;
    bool UpdateBoundsRadius(std::uint32_t networkId, float radius) noexcept;

    /// Count of live mappings (diagnostic).
    [[nodiscard]] std::size_t Size() const noexcept { return m_NetToRender.size(); }

    /// Clear every mapping and destroy all bridged render entities.
    /// Client-side VFX created directly on the RenderWorld are untouched.
    void Clear();

private:
    World::RenderWorld* m_World = nullptr;
    std::unordered_map<std::uint32_t, World::RenderEntityId> m_NetToRender;
};

} // namespace SagaEngine::Render::Bridge
