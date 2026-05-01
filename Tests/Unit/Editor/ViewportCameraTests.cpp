/// @file ViewportCameraTests.cpp
/// @brief GoogleTest coverage for `ViewportCamera` and `CameraController`.

#include "SagaEditor/Viewport/CameraController.h"
#include "SagaEditor/Viewport/ViewportCamera.h"
#include "SagaEditor/Viewport/ViewportInput.h"

#include "SagaEngine/Math/MathConstants.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

namespace
{

using namespace SagaEditor;
using SagaEngine::Math::Vec3;

constexpr float kEps  = 1.0e-4f;
constexpr double kEpsD = 1.0e-9;

[[nodiscard]] bool VecApprox(const Vec3& a, const Vec3& b, float eps = kEps) noexcept
{
    return std::fabs(a.x - b.x) < eps &&
           std::fabs(a.y - b.y) < eps &&
           std::fabs(a.z - b.z) < eps;
}

// ─── ViewportCamera ───────────────────────────────────────────────────────────

TEST(ViewportCameraTest, DefaultCameraLooksDownNegativeZ)
{
    ViewportCamera cam;
    EXPECT_TRUE(VecApprox(cam.GetForward(), Vec3{0.0f, 0.0f, -1.0f}));
    EXPECT_TRUE(VecApprox(cam.GetRight(),   Vec3{1.0f, 0.0f,  0.0f}));
    EXPECT_TRUE(VecApprox(cam.GetUp(),      Vec3{0.0f, 1.0f,  0.0f}));
}

TEST(ViewportCameraTest, PositionDerivedFromOrbitRadius)
{
    ViewportCamera cam;
    cam.pivot       = {0.0f, 0.0f, 0.0f};
    cam.orbitRadius = 5.0f;

    // Default forward = -Z, so camera position = pivot - forward * radius
    // = (0,0,0) - (0,0,-5) = (0,0,5).
    EXPECT_TRUE(VecApprox(cam.GetPosition(), Vec3{0.0f, 0.0f, 5.0f}));
}

TEST(ViewportCameraTest, RotateClampsPitchToHalfPiMinusEpsilon)
{
    ViewportCamera cam;
    cam.Rotate(0.0, 100.0); // far past +pi/2
    EXPECT_LT(cam.pitchRadians,  SagaEngine::Math::kPiD * 0.5);

    cam.Rotate(0.0, -200.0);
    EXPECT_GT(cam.pitchRadians, -SagaEngine::Math::kPiD * 0.5);
}

TEST(ViewportCameraTest, ZoomMultipliesOrbitRadiusAndClamps)
{
    ViewportCamera cam;
    cam.orbitRadius    = 10.0f;
    cam.minOrbitRadius = 1.0f;
    cam.maxOrbitRadius = 100.0f;

    cam.Zoom(0.5f);
    EXPECT_FLOAT_EQ(cam.orbitRadius, 5.0f);

    cam.Zoom(0.01f);
    EXPECT_FLOAT_EQ(cam.orbitRadius, cam.minOrbitRadius);

    cam.Zoom(100000.0f);
    EXPECT_FLOAT_EQ(cam.orbitRadius, cam.maxOrbitRadius);

    cam.Zoom(-1.0f); // ignored
    EXPECT_FLOAT_EQ(cam.orbitRadius, cam.maxOrbitRadius);

    cam.Zoom(0.0f); // ignored
    EXPECT_FLOAT_EQ(cam.orbitRadius, cam.maxOrbitRadius);
}

TEST(ViewportCameraTest, PanMovesPivotByWorldOffset)
{
    ViewportCamera cam;
    cam.pivot = {1.0f, 2.0f, 3.0f};

    cam.Pan(Vec3{0.5f, -1.0f, 2.0f});
    EXPECT_TRUE(VecApprox(cam.pivot, Vec3{1.5f, 1.0f, 5.0f}));
}

TEST(ViewportCameraTest, SnapToAxisFront)
{
    ViewportCamera cam;
    cam.SnapToAxis(AxisSnap::Front);
    EXPECT_EQ(cam.mode, ProjectionMode::Orthographic);
    EXPECT_TRUE(VecApprox(cam.GetForward(), Vec3{0.0f, 0.0f, -1.0f}));
}

TEST(ViewportCameraTest, SnapToAxisRight)
{
    ViewportCamera cam;
    cam.SnapToAxis(AxisSnap::Right);
    EXPECT_EQ(cam.mode, ProjectionMode::Orthographic);
    EXPECT_TRUE(VecApprox(cam.GetForward(), Vec3{-1.0f, 0.0f, 0.0f}));
}

TEST(ViewportCameraTest, SnapToAxisTop)
{
    ViewportCamera cam;
    cam.SnapToAxis(AxisSnap::Top);
    EXPECT_EQ(cam.mode, ProjectionMode::Orthographic);
    // Looking straight down — forward should be approximately (0, -1, 0).
    // Tolerance must exceed the pitch-clamp epsilon (1e-3 rad), since
    // cos(pi/2 - eps) ≈ eps.
    const auto fwd = cam.GetForward();
    EXPECT_NEAR(fwd.x,  0.0f, 5.0e-3f);
    EXPECT_NEAR(fwd.y, -1.0f, 5.0e-3f);
    EXPECT_NEAR(fwd.z,  0.0f, 5.0e-3f);
}

TEST(ViewportCameraTest, FrameBoundsCentersPivotAndExpandsRadius)
{
    ViewportCamera cam;
    cam.fovYRadians = 1.0472f; // 60 deg
    cam.minOrbitRadius = 0.05f;
    cam.maxOrbitRadius = 1.0e6f;

    cam.FrameBounds(Vec3{1.0f, 2.0f, 3.0f}, 4.0f, 16.0f / 9.0f);

    EXPECT_TRUE(VecApprox(cam.pivot, Vec3{1.0f, 2.0f, 3.0f}));
    EXPECT_GT(cam.orbitRadius, 4.0f); // must move back further than the radius.
}

TEST(ViewportCameraTest, FrameBoundsHandlesZeroRadius)
{
    ViewportCamera cam;
    cam.FrameBounds(Vec3{0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
    EXPECT_GT(cam.orbitRadius, 0.0f);
}

TEST(ViewportCameraTest, SanitizeClampsBadInputs)
{
    ViewportCamera cam;
    cam.orbitRadius    = -5.0f;
    cam.fovYRadians    = -1.0f;
    cam.nearPlane      = 0.0f;
    cam.farPlane       = -100.0f;
    cam.orthoHalfHeight = -3.0f;
    cam.pitchRadians   = 1000.0;
    cam.minOrbitRadius = -1.0f;
    cam.maxOrbitRadius = -10.0f;

    EXPECT_TRUE(cam.Sanitize());
    EXPECT_GT(cam.orbitRadius,    0.0f);
    EXPECT_GT(cam.fovYRadians,    0.0f);
    EXPECT_GT(cam.nearPlane,      0.0f);
    EXPECT_GT(cam.farPlane, cam.nearPlane);
    EXPECT_GT(cam.orthoHalfHeight, 0.0f);
    EXPECT_LT(cam.pitchRadians,   SagaEngine::Math::kPiD * 0.5);

    EXPECT_FALSE(cam.Sanitize()); // idempotent on a valid camera.
}

TEST(ViewportCameraTest, GetUpStaysOrthogonalToForwardAfterPitch)
{
    ViewportCamera cam;
    cam.Rotate(0.5, 0.7);

    const auto f = cam.GetForward();
    const auto u = cam.GetUp();
    EXPECT_NEAR(f.Dot(u), 0.0f, kEps);
}

TEST(ViewportCameraTest, DegreesAccessorsRoundTrip)
{
    ViewportCamera cam;
    cam.yawRadians   = SagaEngine::Math::kPiD * 0.25;
    cam.pitchRadians = SagaEngine::Math::kPiD * 0.10;
    EXPECT_NEAR(cam.GetYawDegrees(),   45.0, 1.0e-6);
    EXPECT_NEAR(cam.GetPitchDegrees(), 18.0, 1.0e-6);
    (void)kEpsD;
}

// ─── CameraController ─────────────────────────────────────────────────────────

class CameraControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_camera                 = ViewportCamera{};
        m_camera.pivot           = Vec3{0.0f, 0.0f, 0.0f};
        m_camera.orbitRadius     = 10.0f;
        m_camera.minOrbitRadius  = 0.05f;
        m_camera.maxOrbitRadius  = 1.0e6f;
        m_controller = std::make_unique<CameraController>(&m_camera);
    }

    [[nodiscard]] ViewportInputState BaseInput() const noexcept
    {
        ViewportInputState in;
        in.viewportW    = 1280.0f;
        in.viewportH    = 720.0f;
        in.deltaSeconds = 1.0f / 60.0f;
        return in;
    }

    ViewportCamera                    m_camera;
    std::unique_ptr<CameraController> m_controller;
};

TEST_F(CameraControllerTest, IdleInputLeavesCameraUnchanged)
{
    const ViewportCamera before = m_camera;
    const auto mode = m_controller->Tick(BaseInput());
    EXPECT_EQ(mode, CameraNavigationMode::Idle);
    EXPECT_DOUBLE_EQ(m_camera.yawRadians,   before.yawRadians);
    EXPECT_DOUBLE_EQ(m_camera.pitchRadians, before.pitchRadians);
    EXPECT_FLOAT_EQ(m_camera.orbitRadius,   before.orbitRadius);
}

TEST_F(CameraControllerTest, LeftDragOrbitsCamera)
{
    auto input = BaseInput();
    input.buttons      = MouseButton::Left;
    input.mouseDeltaX  = 50.0f;
    input.mouseDeltaY  = -30.0f;

    const auto mode = m_controller->Tick(input);
    EXPECT_EQ(mode, CameraNavigationMode::Orbit);
    EXPECT_LT(m_camera.yawRadians,   0.0); // negative yaw delta
    EXPECT_GT(m_camera.pitchRadians, 0.0); // mouse up = positive pitch
}

TEST_F(CameraControllerTest, RightDragSwitchesToFlyMode)
{
    auto input = BaseInput();
    input.buttons     = MouseButton::Right;
    input.mouseDeltaX = 10.0f;

    const auto mode = m_controller->Tick(input);
    EXPECT_EQ(mode, CameraNavigationMode::Fly);
}

TEST_F(CameraControllerTest, ShiftLeftDragPansPivot)
{
    auto input = BaseInput();
    input.buttons     = MouseButton::Left;
    input.modifiers   = KeyModifiers::Shift;
    input.mouseDeltaX = 80.0f;
    input.mouseDeltaY = 0.0f;

    const auto before = m_camera.pivot;
    const auto mode   = m_controller->Tick(input);
    EXPECT_EQ(mode, CameraNavigationMode::Pan);
    EXPECT_NE(m_camera.pivot, before);
}

TEST_F(CameraControllerTest, MiddleDragAlsoPans)
{
    auto input = BaseInput();
    input.buttons     = MouseButton::Middle;
    input.mouseDeltaX = 40.0f;

    const auto mode = m_controller->Tick(input);
    EXPECT_EQ(mode, CameraNavigationMode::Pan);
}

TEST_F(CameraControllerTest, ScrollWheelZoomsAndIsModeIndependent)
{
    auto input = BaseInput();
    input.scrollDelta = 1.0f; // forward scroll = zoom in => smaller radius.
    const auto before = m_camera.orbitRadius;

    const auto mode = m_controller->Tick(input);
    EXPECT_EQ(mode, CameraNavigationMode::Idle);
    EXPECT_LT(m_camera.orbitRadius, before);

    auto out = BaseInput();
    out.scrollDelta = -1.0f;
    m_controller->Tick(out);
    EXPECT_NEAR(m_camera.orbitRadius, before, 1.0e-3f);
}

TEST_F(CameraControllerTest, FlyForwardMovesPivotAlongForward)
{
    auto input = BaseInput();
    input.buttons      = MouseButton::Right;
    input.flyMask      = FlyDirection::Forward;
    input.deltaSeconds = 1.0f / 30.0f;

    const auto fwdBefore = m_camera.GetForward();
    const auto pivotBefore = m_camera.pivot;

    m_controller->Tick(input);

    const auto delta = m_camera.pivot - pivotBefore;
    EXPECT_GT(delta.LengthSq(), 0.0f);
    // Movement direction matches forward (allow small drift from the
    // mouse-look component; here we sent zero delta).
    EXPECT_GT(delta.Dot(fwdBefore), 0.0f);
}

TEST_F(CameraControllerTest, FlyShiftMultiplierSpeedsUp)
{
    auto baseInput = BaseInput();
    baseInput.buttons      = MouseButton::Right;
    baseInput.flyMask      = FlyDirection::Forward;
    baseInput.deltaSeconds = 1.0f / 30.0f;

    auto camA = m_camera;
    {
        CameraController c(&camA);
        c.Tick(baseInput);
    }

    auto fastInput = baseInput;
    fastInput.modifiers = KeyModifiers::Shift;
    auto camB = m_camera;
    {
        CameraController c(&camB);
        c.Tick(fastInput);
    }

    const auto distA = (camA.pivot - m_camera.pivot).LengthSq();
    const auto distB = (camB.pivot - m_camera.pivot).LengthSq();
    EXPECT_GT(distB, distA);
}

TEST_F(CameraControllerTest, MouseDeltaIsClampedAgainstLargeJumps)
{
    auto cfg = m_controller->GetConfig();
    cfg.maxPixelsPerTick = 50.0f;
    cfg.orbitSensitivity = 0.005f;
    m_controller->SetConfig(cfg);

    auto huge = BaseInput();
    huge.buttons     = MouseButton::Left;
    huge.mouseDeltaX = 100000.0f;

    m_controller->Tick(huge);
    EXPECT_NEAR(m_camera.yawRadians,
                -50.0 * 0.005, 1.0e-6); // clamp to 50px
}

TEST_F(CameraControllerTest, NullCameraIsHandledGracefully)
{
    CameraController nullController(nullptr);
    auto input = BaseInput();
    input.buttons = MouseButton::Left;
    EXPECT_EQ(nullController.Tick(input), CameraNavigationMode::Idle);
}

TEST_F(CameraControllerTest, FrameBoundsAndSnapForwardToCamera)
{
    m_controller->SnapToAxis(AxisSnap::Top);
    EXPECT_EQ(m_camera.mode, ProjectionMode::Orthographic);

    m_controller->FrameBounds(Vec3{10.0f, 0.0f, 0.0f}, 5.0f, 1.0f);
    EXPECT_TRUE(VecApprox(m_camera.pivot, Vec3{10.0f, 0.0f, 0.0f}));
}

// ─── Input Bitmask Helpers ────────────────────────────────────────────────────

TEST(ViewportInputTest, MouseButtonBitwiseHelpers)
{
    auto m = MouseButton::Left | MouseButton::Right;
    EXPECT_TRUE (HasButton(m, MouseButton::Left));
    EXPECT_TRUE (HasButton(m, MouseButton::Right));
    EXPECT_FALSE(HasButton(m, MouseButton::Middle));

    m |= MouseButton::Middle;
    EXPECT_TRUE(HasButton(m, MouseButton::Middle));
}

TEST(ViewportInputTest, KeyModifierBitwiseHelpers)
{
    auto k = KeyModifiers::Shift | KeyModifiers::Ctrl;
    EXPECT_TRUE (HasModifier(k, KeyModifiers::Shift));
    EXPECT_TRUE (HasModifier(k, KeyModifiers::Ctrl));
    EXPECT_FALSE(HasModifier(k, KeyModifiers::Alt));
}

TEST(ViewportInputTest, FlyDirectionBitwiseHelpers)
{
    auto d = FlyDirection::Forward | FlyDirection::Right | FlyDirection::Up;
    EXPECT_TRUE (HasDirection(d, FlyDirection::Forward));
    EXPECT_TRUE (HasDirection(d, FlyDirection::Right));
    EXPECT_TRUE (HasDirection(d, FlyDirection::Up));
    EXPECT_FALSE(HasDirection(d, FlyDirection::Backward));
}

} // namespace
