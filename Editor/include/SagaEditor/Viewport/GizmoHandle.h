/// @file GizmoHandle.h
/// @brief Identity, hit-testing, and dispatch for transform gizmo handles.

#pragma once

#include "SagaEditor/Viewport/ViewportRay.h"

#include "SagaEngine/Math/Vec3.h"

#include <cstdint>
#include <optional>

namespace SagaEditor
{

// ─── Gizmo Mode ───────────────────────────────────────────────────────────────

/// The three top-level transform gizmo modes the editor supports.
/// `Universal` is reserved for the future combined translate/rotate/
/// scale handle; today the renderer only draws the first three.
enum class GizmoMode : std::uint8_t
{
    Translate,
    Rotate,
    Scale,
    Universal,
};

// ─── Gizmo Handle ─────────────────────────────────────────────────────────────

/// Stable identifier for a single handle inside a gizmo. The enum is
/// dense so the renderer and the input controller can use the value
/// directly as an index into per-handle colour / hot-state arrays.
enum class GizmoHandle : std::uint8_t
{
    None,
    AxisX,           ///< Translate / scale along world or local +X.
    AxisY,           ///< +Y axis.
    AxisZ,           ///< +Z axis.
    PlaneXY,         ///< Translate inside the XY plane.
    PlaneYZ,         ///< Translate inside the YZ plane.
    PlaneXZ,         ///< Translate inside the XZ plane.
    RotationX,       ///< Rotate around +X.
    RotationY,       ///< Rotate around +Y.
    RotationZ,       ///< Rotate around +Z.
    ScaleUniform,    ///< Centre handle on the scale gizmo.
};

/// Stable lowercase string id for `handle`. Used by the layout
/// serializer and by the editor command system. Returns an empty
/// view for `GizmoHandle::None`.
[[nodiscard]] const char* GizmoHandleId(GizmoHandle handle) noexcept;

// ─── Handle Hit Result ────────────────────────────────────────────────────────

/// Description of a successful gizmo handle hit. The renderer passes
/// the value back into the gizmo controller during a drag so the
/// controller knows which handle it is mutating.
struct GizmoHandleHit
{
    GizmoHandle  handle  = GizmoHandle::None;
    float        rayT    = 0.0f;  ///< Ray parameter at the hit.
    float        distance = 0.0f; ///< World-space distance to the handle.
};

// ─── Handle Hit-Test ──────────────────────────────────────────────────────────

/// Configuration for `HitTestTranslateGizmo`. The controller adjusts
/// the screen-space tolerance based on the active theme and the
/// viewport size, so the value is exposed rather than baked in.
struct GizmoHitConfig
{
    /// Length of each axis arrow in world units.
    float axisLength            = 1.0f;

    /// Half-extent of each plane handle in world units (the plane
    /// handle is a square anchored at the gizmo origin).
    float planeHalfExtent       = 0.25f;

    /// World-space tolerance for axis hits. The controller computes
    /// this from the viewport pixel size and the camera distance so
    /// that the handle always feels approximately the same width on
    /// screen.
    float axisHitTolerance      = 0.04f;

    /// Maximum distance from a plane handle accepted as a hit.
    float planeHitTolerance     = 0.04f;
};

/// Hit-test the translate gizmo at `gizmoOrigin` against `ray`. Tests
/// the three axis segments (+X / +Y / +Z) and the three plane
/// handles. Returns the closest hit by ray parameter, or
/// `std::nullopt` when the ray missed every handle. Behind-the-camera
/// hits (negative `t`) are rejected.
[[nodiscard]] std::optional<GizmoHandleHit>
    HitTestTranslateGizmo(const ViewportRay&             ray,
                          const SagaEngine::Math::Vec3&  gizmoOrigin,
                          const GizmoHitConfig&          config) noexcept;

/// Hit-test the rotate gizmo. Tests the three rotation rings
/// (X / Y / Z); a hit is recorded when the ray pierces the plane the
/// ring lives in within `axisHitTolerance` of the ring radius.
[[nodiscard]] std::optional<GizmoHandleHit>
    HitTestRotateGizmo(const ViewportRay&             ray,
                       const SagaEngine::Math::Vec3&  gizmoOrigin,
                       const GizmoHitConfig&          config) noexcept;

/// Hit-test the scale gizmo. Tests the three axis segments plus the
/// uniform-scale centre handle. The centre handle wins when the ray
/// passes through a sphere of radius `axisHitTolerance * 1.5`.
[[nodiscard]] std::optional<GizmoHandleHit>
    HitTestScaleGizmo(const ViewportRay&             ray,
                      const SagaEngine::Math::Vec3&  gizmoOrigin,
                      const GizmoHitConfig&          config) noexcept;

} // namespace SagaEditor
