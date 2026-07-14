/// @file CameraController.cpp
/// @brief Implementation of the viewport camera controller.

#include "SagaEditor/Viewport/CameraController.h"

#include <algorithm>
#include <cmath>

namespace SagaEditor
{

namespace
{

// ─── Helpers ──────────────────────────────────────────────────────────────────

[[nodiscard]] inline float ClampDelta(float v, float maxAbs) noexcept
{
    if (v >  maxAbs) return  maxAbs;
    if (v < -maxAbs) return -maxAbs;
    return v;
}

[[nodiscard]] inline float SafeDiv(float n, float d) noexcept
{
    return d > 0.0f ? n / d : 0.0f;
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

CameraController::CameraController(ViewportCamera* camera) noexcept
    : m_camera(camera)
{}

// ─── Configuration ────────────────────────────────────────────────────────────

void CameraController::SetConfig(const CameraControllerConfig& cfg) noexcept
{
    m_config = cfg;
}

const CameraControllerConfig& CameraController::GetConfig() const noexcept
{
    return m_config;
}

// ─── Mode Resolution ──────────────────────────────────────────────────────────

CameraNavigationMode
CameraController::ResolveMode(const ViewportInputState& input) const noexcept
{
    if (HasButton(input.buttons, MouseButton::Right))
    {
        return CameraNavigationMode::Fly;
    }
    if (HasButton(input.buttons, MouseButton::Middle) ||
        (HasButton(input.buttons, MouseButton::Left) &&
         HasModifier(input.modifiers, KeyModifiers::Shift)))
    {
        return CameraNavigationMode::Pan;
    }
    if (HasButton(input.buttons, MouseButton::Left))
    {
        return CameraNavigationMode::Orbit;
    }
    return CameraNavigationMode::Idle;
}

// ─── Per-Mode Mutations ───────────────────────────────────────────────────────

void CameraController::ApplyOrbit(const ViewportInputState& input) noexcept
{
    const float dx = ClampDelta(input.mouseDeltaX, m_config.maxPixelsPerTick);
    const float dy = ClampDelta(input.mouseDeltaY, m_config.maxPixelsPerTick);

    // Yaw is positive when the cursor moves left so the camera
    // appears to follow the cursor; pitch is positive when the cursor
    // moves up.
    const double yawDelta   = -dx * m_config.orbitSensitivity;
    const double pitchDelta = -dy * m_config.orbitSensitivity;

    m_camera->Rotate(yawDelta, pitchDelta);
}

void CameraController::ApplyPan(const ViewportInputState& input) noexcept
{
    const float dx = ClampDelta(input.mouseDeltaX, m_config.maxPixelsPerTick);
    const float dy = ClampDelta(input.mouseDeltaY, m_config.maxPixelsPerTick);

    // Pan distance scales with orbit radius so panning while zoomed
    // out covers more ground than panning while zoomed in.
    const float scale = m_config.panSensitivity * m_camera->orbitRadius;

    const auto right = m_camera->GetRight();
    const auto up    = m_camera->GetUp();

    // Cursor moving right should drag the world right -> pivot moves
    // left in world space; cursor moving down should drag the world
    // down -> pivot moves up.
    const ViewportCamera::Vec3 offset{
        right.x * (-dx) * scale + up.x *  dy * scale,
        right.y * (-dx) * scale + up.y *  dy * scale,
        right.z * (-dx) * scale + up.z *  dy * scale,
    };

    m_camera->Pan(offset);
}

void CameraController::ApplyFly(const ViewportInputState& input) noexcept
{
    // Mouse-look component first so translation happens against the
    // updated basis.
    const float dx = ClampDelta(input.mouseDeltaX, m_config.maxPixelsPerTick);
    const float dy = ClampDelta(input.mouseDeltaY, m_config.maxPixelsPerTick);

    const double yawDelta   = -dx * m_config.flyLookSensitivity;
    const double pitchDelta = -dy * m_config.flyLookSensitivity;
    if (yawDelta != 0.0 || pitchDelta != 0.0)
    {
        m_camera->Rotate(yawDelta, pitchDelta);
    }

    if (input.flyMask == FlyDirection::None || input.deltaSeconds <= 0.0f)
    {
        return;
    }

    float speed = m_config.flyBaseSpeed;
    if (HasModifier(input.modifiers, KeyModifiers::Shift))
    {
        speed *= m_config.flyShiftMultiplier;
    }
    if (HasModifier(input.modifiers, KeyModifiers::Ctrl))
    {
        speed *= m_config.flyCtrlMultiplier;
    }

    const float step = speed * input.deltaSeconds;

    const auto fwd   = m_camera->GetForward();
    const auto right = m_camera->GetRight();
    const auto up    = m_camera->GetUp();

    ViewportCamera::Vec3 move{0.0f, 0.0f, 0.0f};
    if (HasDirection(input.flyMask, FlyDirection::Forward))  move += fwd   *  step;
    if (HasDirection(input.flyMask, FlyDirection::Backward)) move += fwd   * -step;
    if (HasDirection(input.flyMask, FlyDirection::Right))    move += right *  step;
    if (HasDirection(input.flyMask, FlyDirection::Left))     move += right * -step;
    if (HasDirection(input.flyMask, FlyDirection::Up))       move += up    *  step;
    if (HasDirection(input.flyMask, FlyDirection::Down))     move += up    * -step;

    if (move.LengthSq() > 0.0f)
    {
        m_camera->Pan(move);
    }
}

void CameraController::ApplyZoom(const ViewportInputState& input) noexcept
{
    if (input.scrollDelta == 0.0f)
    {
        return;
    }

    // Each tick scales orbit radius by zoomFactorPerTick. A positive
    // scroll value means "wheel rolled forward" => zoom in =>
    // factor < 1.0 => smaller radius.
    const float factor = std::pow(m_config.zoomFactorPerTick, input.scrollDelta);
    m_camera->Zoom(factor);
}

// ─── Per-Frame Update ─────────────────────────────────────────────────────────

CameraNavigationMode
CameraController::Tick(const ViewportInputState& input) noexcept
{
    if (m_camera == nullptr)
    {
        return CameraNavigationMode::Idle;
    }

    m_camera->Sanitize();
    (void)SafeDiv; // reserved for resolution-relative scaling later.

    // Scroll-wheel zoom is independent of the active drag mode.
    ApplyZoom(input);

    m_activeMode = ResolveMode(input);

    switch (m_activeMode)
    {
        case CameraNavigationMode::Orbit: ApplyOrbit(input); break;
        case CameraNavigationMode::Pan:   ApplyPan  (input); break;
        case CameraNavigationMode::Fly:   ApplyFly  (input); break;
        case CameraNavigationMode::Idle:  /* nothing */      break;
    }

    return m_activeMode;
}

CameraNavigationMode CameraController::GetActiveMode() const noexcept
{
    return m_activeMode;
}

// ─── Discrete Commands ────────────────────────────────────────────────────────

void CameraController::FrameBounds(const ViewportCamera::Vec3& worldCentre,
                                   float                       boundsRadius,
                                   float                       aspect) noexcept
{
    if (m_camera != nullptr)
    {
        m_camera->FrameBounds(worldCentre, boundsRadius, aspect);
    }
}

void CameraController::SnapToAxis(AxisSnap axis) noexcept
{
    if (m_camera != nullptr)
    {
        m_camera->SnapToAxis(axis);
    }
}

} // namespace SagaEditor
