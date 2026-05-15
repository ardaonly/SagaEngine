/// @file RenderEntity.h
/// @brief Handle + data for a single drawable the client-side render world owns.
///
/// Layer  : SagaEngine / Render / World
/// Purpose: The render world is deliberately decoupled from the gameplay ECS.
///          Gameplay code never touches a RenderEntity directly — it feeds
///          replicated state through a NetEntityBinding, which turns it
///          into RenderEntity records. Keeping this boundary strict is why
///          a network hiccup cannot freeze the camera, and why dedicated
///          server or replay nodes can skip the render world entirely.
///
/// Design rules:
///   - RenderEntityId is a stable opaque handle. IDs are never recycled
///     within a session, so dangling references fail loudly rather than
///     aliasing into the wrong entity.
///   - RenderEntity is POD-like. No raw pointers, no GPU handles — only
///     value-type data. GPU resources live behind MeshId / MaterialId
///     handles resolved by the backend.
///   - Bounds are stored as a world-space sphere (centre + radius). A
///     sphere is cheaper to cull and perfectly adequate for the first
///     culling pass. A tighter AABB can be added per-entity later if we
///     ever need it.

#pragma once

#include "SagaEngine/Math/Transform.h"

#include <cstdint>

namespace SagaEngine::Render::World
{

// ─── Handles ──────────────────────────────────────────────────────────

/// Stable handle into RenderWorld's entity pool. Never recycled.
enum class RenderEntityId : std::uint32_t { kInvalid = 0xFFFFFFFFu };

/// Handle into the backend's mesh resource table.
enum class MeshId : std::uint32_t { kInvalid = 0xFFFFFFFFu };

/// Handle into the backend's material resource table (runtime form).
enum class MaterialId : std::uint32_t { kInvalid = 0xFFFFFFFFu };

// ─── Visibility groups ────────────────────────────────────────────────

/// 32 bits of layer membership. Cameras declare a visibility filter; an
/// entity is only drawn through a given camera if (entity.groups &
/// camera.filter) != 0. Typical groups: world geometry, characters,
/// VFX, editor gizmos, minimap-only, first-person-only, etc.
using VisibilityMask = std::uint32_t;

inline constexpr VisibilityMask kVisibilityAll  = 0xFFFFFFFFu;
inline constexpr VisibilityMask kVisibilityNone = 0u;

// ─── RenderEntity ─────────────────────────────────────────────────────

/// Sentinel LOD level meaning "let the LOD selector decide".
inline constexpr std::uint8_t kLodOverrideAuto = 0xFFu;

/// Value-type record for one drawable instance. The render world stores
/// these in a contiguous pool keyed by RenderEntityId.
struct RenderEntity
{
    ::SagaEngine::Math::Transform transform{};     ///< World-space transform.
    MeshId          mesh            = MeshId::kInvalid;
    MaterialId      material        = MaterialId::kInvalid;
    float           boundsRadius    = 0.0f;          ///< World-space sphere radius.
    VisibilityMask  visibilityMask  = kVisibilityAll;
    std::uint8_t    lodOverride     = kLodOverrideAuto;
    bool            visible         = true;          ///< Artist / server hide flag.

    /// Optional link back to the replicated entity that spawned this
    /// render entity. 0 means "this render entity is purely client-side
    /// (VFX, decal, billboard marker)". The render world itself does not
    /// read this field — NetEntityBinding maintains it.
    std::uint32_t   networkId       = 0;
};

/// A RenderEntity is exactly what the CPU-side culling + LOD pipeline
/// reads. Keep it small enough that a batch of a few thousand fits in
/// L2 during a single cull pass. The static assertion is advisory —
/// inflate it deliberately if a field genuinely belongs here.
static_assert(sizeof(RenderEntity) <= 128,
              "RenderEntity grew — confirm the extra field really belongs here");

} // namespace SagaEngine::Render::World
