/// @file ViewportCamera.h
/// @brief Pure camera state for the editor viewport — no RHI dependency.

#pragma once

#include "SagaEngine/Math/Vec3.h"

#include <cstdint>

namespace SagaEditor
{

// ─── Projection Mode ──────────────────────────────────────────────────────────

/// The two projection types the editor viewport supports. Orthographic
/// projection is used by the top / front / right snap views and by
/// schematic editing modes that disable perspective foreshortening.
enum class ProjectionMode : std::uint8_t
{
    Perspective,
    Orthographic,
};

// ─── Snap Direction ───────────────────────────────────────────────────────────

/// Convenience enum used by `SnapToAxis`. Each entry produces a
/// camera looking down the named world axis; the orbit centre is
/// preserved across the snap.
enum class AxisSnap : std::uint8_t
{
    Top,    ///< Camera at +Y looking down.
    Bottom, ///< Camera at -Y looking up.
    Front,  ///< Camera at +Z looking down -Z.
    Back,   ///< Camera at -Z looking down +Z.
    Right,  ///< Camera at +X looking down -X.
    Left,   ///< Camera at -X looking down +X.
};

// ─── Viewport Camera ──────────────────────────────────────────────────────────

/// Pure camera value type used by the editor viewport. The camera
/// holds an orbit pivot, a yaw / pitch orientation, an orbit radius
/// (used by `Perspective` orbit and by all `Orthographic` modes), and
/// the projection parameters. The implementation deliberately does
/// not depend on the engine's `Quat` or `Mat4` types — yaw and pitch
/// are stored as doubles so accumulating drag deltas across many
/// frames does not lose precision; matrix construction is delegated
/// to consumers that pull the camera basis through the helpers below.
class ViewportCamera
{
public:
    using Vec3 = SagaEngine::Math::Vec3;

    // ─── Configuration ────────────────────────────────────────────────────────

    /// Projection mode used the next time a matrix is queried.
    ProjectionMode mode = ProjectionMode::Perspective;

    /// World-space pivot point the camera orbits around.
    Vec3 pivot{0.0f, 0.0f, 0.0f};

    /// Distance from the pivot to the camera position. Always
    /// positive; the controller clamps it on every mutation.
    float orbitRadius = 5.0f;

    /// Yaw in radians, measured CCW around the world +Y axis from the
    /// world +Z axis. Stored as double so per-frame deltas accumulate
    /// cleanly.
    double yawRadians = 0.0;

    /// Pitch in radians, clamped to `[-pi/2 + epsilon, pi/2 - epsilon]`
    /// so the camera never gimbal-locks straight up or down.
    double pitchRadians = 0.0;

    /// Field of view (vertical) in radians. Used in perspective mode.
    float fovYRadians = 1.047197551f; ///< 60 degrees.

    /// Half-extent (in world units) used as the orthographic frustum
    /// vertical extent. The horizontal extent is derived from the
    /// viewport aspect ratio.
    float orthoHalfHeight = 5.0f;

    /// Near and far clip-plane distances in world units. Both must be
    /// strictly positive and `nearPlane < farPlane`.
    float nearPlane = 0.05f;
    float farPlane  = 5000.0f;

    // ─── Limits ───────────────────────────────────────────────────────────────

    /// Smallest allowed orbit radius. Prevents the perspective camera
    /// from passing through the pivot point and inverting the view.
    float minOrbitRadius = 0.05f;

    /// Largest allowed orbit radius. Stops the camera from drifting
    /// out into floating-point territory where depth resolution
    /// collapses.
    float maxOrbitRadius = 1.0e6f;

    // ─── Derived Camera Basis ─────────────────────────────────────────────────

    /// World-space camera position derived from `pivot`, `orbitRadius`,
    /// `yawRadians`, and `pitchRadians`. Always recomputed — never
    /// cached — so consumers see the latest mutation immediately.
    [[nodiscard]] Vec3 GetPosition() const noexcept;

    /// World-space forward direction (unit length, points from camera
    /// position towards the pivot). The engine convention is
    /// right-handed with `-Z` as the default forward, so this matches
    /// `Vec3::Forward()` when the camera is at world origin looking
    /// down `-Z`.
    [[nodiscard]] Vec3 GetForward() const noexcept;

    /// World-space right vector (unit length). Recomputed on every
    /// call from yaw alone, ignoring pitch, so right always stays in
    /// the world XZ plane.
    [[nodiscard]] Vec3 GetRight() const noexcept;

    /// World-space up vector (unit length). Recomputed from forward
    /// and right; matches `Vec3::Up()` when pitch is zero.
    [[nodiscard]] Vec3 GetUp() const noexcept;

    // ─── State Queries ────────────────────────────────────────────────────────

    /// Yaw in degrees, primarily for inspector display.
    [[nodiscard]] double GetYawDegrees()   const noexcept;

    /// Pitch in degrees, primarily for inspector display.
    [[nodiscard]] double GetPitchDegrees() const noexcept;

    // ─── Mutators ─────────────────────────────────────────────────────────────

    /// Add yaw / pitch deltas (in radians) to the current orientation,
    /// clamping pitch to the safe range. Used by the orbit and fly-cam
    /// controllers.
    void Rotate(double deltaYawRadians,
                double deltaPitchRadians) noexcept;

    /// Multiply the orbit radius by `factor` and clamp to
    /// `[minOrbitRadius, maxOrbitRadius]`. Factor must be positive;
    /// non-positive factors are ignored.
    void Zoom(float factor) noexcept;

    /// Translate the pivot by `worldOffset`. The camera position
    /// follows because the orbit basis is rebuilt from `pivot`.
    void Pan(const Vec3& worldOffset) noexcept;

    /// Reset orbit radius without changing pivot or orientation.
    void SetOrbitRadius(float radius) noexcept;

    /// Snap the camera to a canonical axis view. Preserves `pivot`
    /// and `orbitRadius`; switches `mode` to `Orthographic`.
    void SnapToAxis(AxisSnap axis) noexcept;

    /// Frame the camera so a sphere of radius `boundsRadius` centred
    /// at `worldCentre` fits inside the viewport. The camera looks at
    /// the sphere centre along the current forward vector. `aspect`
    /// is `viewportWidth / viewportHeight`. A non-positive radius
    /// becomes `1.0` so callers do not have to special-case empty
    /// selections.
    void FrameBounds(const Vec3& worldCentre,
                     float       boundsRadius,
                     float       aspect) noexcept;

    // ─── Validation ───────────────────────────────────────────────────────────

    /// Repair any invariants the caller may have broken (negative
    /// radius, near >= far, fov out of range, etc.). Called by the
    /// camera controller before reading state every tick. Returns
    /// true when a clamp actually altered any field.
    bool Sanitize() noexcept;
};

} // namespace SagaEditor
