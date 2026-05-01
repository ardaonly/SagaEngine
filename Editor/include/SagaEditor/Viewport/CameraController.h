/// @file CameraController.h
/// @brief Maps viewport input state onto `ViewportCamera` mutations.

#pragma once

#include "SagaEditor/Viewport/ViewportCamera.h"
#include "SagaEditor/Viewport/ViewportInput.h"

#include <cstdint>

namespace SagaEditor
{

// ─── Controller Mode ──────────────────────────────────────────────────────────

/// Three top-level navigation modes the controller switches between
/// based on which mouse button is held when a drag begins. The modes
/// are visible to the inspector overlay so the user gets a hint
/// telling them which behaviour is active.
enum class CameraNavigationMode : std::uint8_t
{
    Idle,   ///< No drag in progress.
    Orbit,  ///< Left mouse: tumble around the pivot.
    Pan,    ///< Middle mouse / Shift+Left: drag the pivot in screen space.
    Fly,    ///< Right mouse: yaw / pitch + WASDQE translation.
};

// ─── Controller Configuration ─────────────────────────────────────────────────

/// Tunables exposed to the editor's preference UI. The defaults match
/// the average expectation of an editor user coming from Unity / Unreal
/// / Blender — small enough increments to feel precise, large enough
/// to traverse a city block in a few seconds.
struct CameraControllerConfig
{
    /// Radians of yaw / pitch produced per pixel of mouse delta in
    /// orbit mode. The controller divides delta by viewport size so
    /// the look-feel is resolution-independent.
    float orbitSensitivity = 0.005f;

    /// Same in fly mode; tuned a bit slower because fly-cam delta is
    /// integrated against a wall-clock-time budget too.
    float flyLookSensitivity = 0.003f;

    /// Pan distance (in world units) per pixel of mouse delta. Scaled
    /// at runtime by `orbitRadius` so panning at long range covers
    /// more ground than panning up close.
    float panSensitivity = 0.0015f;

    /// Multiplier applied per scroll-wheel tick. `0.9` means each tick
    /// brings the camera 10% closer; the inverse applies on scroll-out.
    float zoomFactorPerTick = 0.9f;

    /// World-units-per-second base translation rate for fly-cam.
    /// Modifier keys multiply this by `flyShiftMultiplier` /
    /// `flyCtrlMultiplier`.
    float flyBaseSpeed = 8.0f;

    /// Multiplier applied to fly-cam translation while Shift is held.
    float flyShiftMultiplier = 4.0f;

    /// Multiplier applied to fly-cam translation while Ctrl is held.
    /// Used as a "creep" mode to inspect tight geometry.
    float flyCtrlMultiplier = 0.25f;

    /// Limit on absolute mouse delta consumed per tick. Stops a
    /// stutter-induced 600px frame from spinning the camera by half
    /// a turn.
    float maxPixelsPerTick = 200.0f;
};

// ─── Camera Controller ────────────────────────────────────────────────────────

/// Stateful controller that translates a stream of
/// `ViewportInputState` snapshots into `ViewportCamera` mutations.
/// The controller owns no Qt or RHI state; it sits cleanly between
/// the panel that produces input snapshots and the camera value type
/// that the renderer consumes. Callers tick the controller once per
/// frame; the controller decides which navigation mode applies to
/// the current input and applies the matching mutation in place.
class CameraController
{
public:
    /// Construct a controller that mutates `camera` on every `Tick`.
    /// The pointer must outlive the controller; the controller never
    /// deletes the camera and never holds the pointer beyond the call
    /// chain that constructed it.
    explicit CameraController(ViewportCamera* camera) noexcept;

    // ─── Configuration ────────────────────────────────────────────────────────

    /// Replace the active configuration. Settings take effect on the
    /// next tick; settings touching ongoing drag state (sensitivity)
    /// apply mid-drag without restarting the gesture.
    void SetConfig(const CameraControllerConfig& cfg) noexcept;

    /// Snapshot of the current configuration. Useful for the editor
    /// preferences panel to reflect runtime adjustments.
    [[nodiscard]] const CameraControllerConfig& GetConfig() const noexcept;

    // ─── Per-Frame Update ─────────────────────────────────────────────────────

    /// Apply `input` to the camera. Returns the navigation mode the
    /// controller resolved this tick — the panel uses the return
    /// value to update the cursor shape and the status-bar overlay.
    CameraNavigationMode Tick(const ViewportInputState& input) noexcept;

    /// Mode active during the most recent `Tick`. Reset to `Idle`
    /// when no buttons are held.
    [[nodiscard]] CameraNavigationMode GetActiveMode() const noexcept;

    // ─── Discrete Commands ────────────────────────────────────────────────────

    /// Frame the supplied bounds in the viewport. Forwards to
    /// `ViewportCamera::FrameBounds`; exposed here so the panel can
    /// invoke it from the F-key shortcut without reaching into the
    /// camera directly.
    void FrameBounds(const ViewportCamera::Vec3& worldCentre,
                     float                       boundsRadius,
                     float                       aspect) noexcept;

    /// Snap to a canonical axis view via the camera. Same rationale
    /// as `FrameBounds` — keeps the panel-facing surface narrow.
    void SnapToAxis(AxisSnap axis) noexcept;

private:
    // ─── Internal Helpers ─────────────────────────────────────────────────────

    [[nodiscard]] CameraNavigationMode
        ResolveMode(const ViewportInputState& input) const noexcept;

    void ApplyOrbit(const ViewportInputState& input) noexcept;
    void ApplyPan  (const ViewportInputState& input) noexcept;
    void ApplyFly  (const ViewportInputState& input) noexcept;
    void ApplyZoom (const ViewportInputState& input) noexcept;

    // ─── State ────────────────────────────────────────────────────────────────

    ViewportCamera*        m_camera     = nullptr;        ///< Non-owning.
    CameraControllerConfig m_config;                      ///< Tunables.
    CameraNavigationMode   m_activeMode = CameraNavigationMode::Idle;
};

} // namespace SagaEditor
