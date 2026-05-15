/// @file TransformComponent.h
/// @brief ECS component wrapping Math::Transform for spatial queries and cell splitting.
///
/// Layer  : SagaEngine / ECS / Components
/// Purpose: Provides a standard Transform component that entities can attach to
///          so the world partitioning / cell splitting subsystem can query
///          entity positions.  The component wraps Math::Transform (the engine's
///          common currency for position / rotation / scale) and adds a dirty
///          tracking mask for selective replication.
///
/// Usage:
///   1. Register the component at startup:
///        SAGA_REGISTER_COMPONENT(TransformComponent, 1);
///   2. Add to an entity:
///        world.AddComponent<TransformComponent>(entityId, {position, rotation, scale});
///   3. Query position for cell splitting:
///        auto* tc = world.GetComponent<TransformComponent>(entityId);
///        CellCoord coord = CellCoordFromWorld(tc->transform.position.x,
///                                             tc->transform.position.z, cellSize);
///
/// Thread safety: component data is not thread-safe; callers must hold the
///                ECS write lock when modifying fields.

#pragma once

#include "SagaEngine/Math/Transform.h"
#include "SagaEngine/ECS/PartialComponentDirty.h"

#include <cstdint>

namespace SagaEngine::ECS {

// ─── TransformComponent ──────────────────────────────────────────────────────

/// ECS component for entity transform (position, rotation, scale).
///
/// Field layout for PartialDirtyMask:
///   kPosition = 0  (Vec3 — position)
///   kRotation = 1  (Quat — orientation)
///   kScale    = 2  (Vec3 — per-axis scale)
///   kFieldCount = 3
struct alignas(8) TransformComponent
{
    // Field indices for partial dirty tracking.
    enum Field : unsigned
    {
        kPosition = 0,
        kRotation = 1,
        kScale    = 2,
        kFieldCount
    };
    SAGA_STATIC_FIELD_LIMIT(TransformComponent);

    Math::Transform   transform;        ///< Combined position / rotation / scale.
    PartialDirtyMask  dirty;            ///< Field-level dirty bits for replication.

    // ── Convenience accessors ─────────────────────────────────────────────

    /// Mark position as dirty.
    constexpr void MarkPositionDirty() noexcept { dirty.Mark(kPosition); }

    /// Mark rotation as dirty.
    constexpr void MarkRotationDirty() noexcept { dirty.Mark(kRotation); }

    /// Mark scale as dirty.
    constexpr void MarkScaleDirty() noexcept    { dirty.Mark(kScale);    }

    /// Mark all fields dirty (e.g. teleport, respawn).
    constexpr void MarkAllDirty() noexcept       { dirty.MarkAll();       }
};

static_assert(std::is_trivially_copyable_v<TransformComponent>,
              "TransformComponent must be trivially copyable for serialization.");

} // namespace SagaEngine::ECS
