/// @file InputSubsystemTests.cpp
/// @brief Unit tests for the full client-side input pipeline:
///        InputDevice → InputState → InputActionMap → GameplayInputFrame

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>

#include "SagaEngine/Input/Types/InputTypes.h"
#include "SagaEngine/Input/Core/InputState.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"
#include "SagaEngine/Input/Devices/KeyboardDevice.h"
#include "SagaEngine/Input/Devices/MouseDevice.h"
#include "SagaEngine/Input/Devices/GamepadDevice.h"
#include "SagaEngine/Input/Core/DeviceRegistry.h"
#include "SagaEngine/Input/Mapping/InputAction.h"
#include "SagaEngine/Input/Mapping/InputActionMap.h"
#include "SagaEngine/Input/Mapping/GameplayInputFrame.h"

using namespace SagaEngine::Input;

// ─── Project action IDs ───────────────────────────────────────────────────────

enum class Action : InputActionId
{
    MoveForward = 1,
    MoveBack    = 2,
    MoveLeft    = 3,
    MoveRight   = 4,
    Jump        = 5,
    Fire        = 6,
    ShiftFire   = 7,   // Shift + MouseLeft
    Sprint      = 8,
    LookX       = 9,
    LookY       = 10,
};

static InputActionId A(Action a) { return static_cast<InputActionId>(a); }

// ─── Helper: build a raw frame with key events ────────────────────────────────

static RawInputFrame MakeKeyFrame(
    std::vector<std::pair<KeyCode, bool>> keys,
    uint64_t ts = 1000)
{
    RawInputFrame frame;
    frame.frameTimestampUs = ts;
    for (const auto& [code, pressed] : keys)
        frame.keys.push_back({ code, pressed, /*repeat=*/false,
                               ModifierFlags::None, ts });
    return frame;
}

static RawInputFrame MakeMouseFrame(int relX, int relY,
                                    MouseButton btn = MouseButton::Unknown,
                                    bool pressed = false,
                                    uint64_t ts = 1000)
{
    RawInputFrame frame;
    frame.frameTimestampUs = ts;
    if (relX != 0 || relY != 0)
        frame.mouseMoves.push_back({ 0, 0, relX, relY, ts });
    if (btn != MouseButton::Unknown)
        frame.mouseButtons.push_back({ btn, pressed, 0, 0, 1, ts });
    return frame;
}

// ─── KeyboardDevice tests ─────────────────────────────────────────────────────

class KeyboardDeviceTest : public ::testing::Test
{
protected:
    KeyboardDevice kbd{ 1 };
};

TEST_F(KeyboardDeviceTest, InitialStateAllUp)
{
    EXPECT_FALSE(kbd.GetState().keyboard.IsDown(KeyCode::W));
    EXPECT_FALSE(kbd.GetState().keyboard.WasJustPressed(KeyCode::W));
}

TEST_F(KeyboardDeviceTest, KeyDownRegistered)
{
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::W, true }}));

    EXPECT_TRUE(kbd.GetState().keyboard.IsDown(KeyCode::W));
    EXPECT_TRUE(kbd.GetState().keyboard.WasJustPressed(KeyCode::W));
    EXPECT_FALSE(kbd.GetState().keyboard.WasJustReleased(KeyCode::W));
}

TEST_F(KeyboardDeviceTest, KeyHeldSecondFrame)
{
    // Frame 1: press
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::W, true }}));

    // Frame 2: still down (no new events → key state persists)
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::W, true }}));

    EXPECT_TRUE (kbd.GetState().keyboard.IsDown(KeyCode::W));
    EXPECT_FALSE(kbd.GetState().keyboard.WasJustPressed(KeyCode::W));  // not new
}

TEST_F(KeyboardDeviceTest, KeyReleased)
{
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::Space, true }}));

    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::Space, false }}));

    EXPECT_FALSE(kbd.GetState().keyboard.IsDown(KeyCode::Space));
    EXPECT_TRUE (kbd.GetState().keyboard.WasJustReleased(KeyCode::Space));
}

TEST_F(KeyboardDeviceTest, ModifierFlagsTracked)
{
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({{ KeyCode::LShift, true }}));

    EXPECT_TRUE(HasModifier(kbd.GetActiveModifiers(), ModifierFlags::Shift));
    EXPECT_FALSE(HasModifier(kbd.GetActiveModifiers(), ModifierFlags::Ctrl));
}

TEST_F(KeyboardDeviceTest, MultipleKeysSimultaneous)
{
    kbd.BeginFrame();
    kbd.ConsumeFrame(MakeKeyFrame({
        { KeyCode::W, true },
        { KeyCode::LShift, true },
        { KeyCode::Space, true },
    }));

    EXPECT_TRUE(kbd.GetState().keyboard.IsDown(KeyCode::W));
    EXPECT_TRUE(kbd.GetState().keyboard.IsDown(KeyCode::LShift));
    EXPECT_TRUE(kbd.GetState().keyboard.IsDown(KeyCode::Space));
}

// ─── MouseDevice tests ────────────────────────────────────────────────────────

class MouseDeviceTest : public ::testing::Test
{
protected:
    MouseDevice mouse{ 2 };
};

TEST_F(MouseDeviceTest, RelDeltaAccumulated)
{
    // Two move events in one frame — both deltas should accumulate.
    RawInputFrame frame;
    frame.frameTimestampUs = 1000;
    frame.mouseMoves.push_back({ 100, 50,  5, -3, 1000 });
    frame.mouseMoves.push_back({ 105, 47, 10,  2, 1001 });

    mouse.BeginFrame();
    mouse.ConsumeFrame(frame);

    EXPECT_EQ(mouse.GetRelX(), 15);
    EXPECT_EQ(mouse.GetRelY(), -1);
    EXPECT_EQ(mouse.GetAbsX(), 105);  // last absolute wins
    EXPECT_EQ(mouse.GetAbsY(), 47);
}

TEST_F(MouseDeviceTest, RelDeltaResetsEachFrame)
{
    mouse.BeginFrame();
    mouse.ConsumeFrame(MakeMouseFrame(10, 5));

    mouse.BeginFrame();  // FlipFrame resets rel delta
    mouse.ConsumeFrame(MakeMouseFrame(0, 0));

    EXPECT_EQ(mouse.GetRelX(), 0);
    EXPECT_EQ(mouse.GetRelY(), 0);
}

TEST_F(MouseDeviceTest, ButtonDownAndEdgeDetection)
{
    mouse.BeginFrame();
    mouse.ConsumeFrame(MakeMouseFrame(0, 0, MouseButton::Left, true));

    EXPECT_TRUE(mouse.GetState().mouse.IsButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.GetState().mouse.WasButtonJustPressed(MouseButton::Left));
}

TEST_F(MouseDeviceTest, ScrollAccumulated)
{
    RawInputFrame frame;
    frame.frameTimestampUs = 1000;
    frame.mouseScrolls.push_back({ 0.f, 1.f, false, 1000 });
    frame.mouseScrolls.push_back({ 0.f, 2.f, false, 1001 });

    mouse.BeginFrame();
    mouse.ConsumeFrame(frame);

    EXPECT_FLOAT_EQ(mouse.GetScrollY(), 3.f);
}

// ─── GamepadDevice tests ──────────────────────────────────────────────────────

class GamepadDeviceTest : public ::testing::Test
{
protected:
    GamepadDevice gp{ 1000 };
};

TEST_F(GamepadDeviceTest, DisconnectedByDefault)
{
    EXPECT_FALSE(gp.IsConnected());
}

TEST_F(GamepadDeviceTest, ConnectsViaFrame)
{
    RawInputFrame frame;
    frame.frameTimestampUs = 1000;
    frame.gamepadConnections.push_back({ 1000, true, 1000 });

    gp.BeginFrame();
    gp.ConsumeFrame(frame);

    EXPECT_TRUE(gp.IsConnected());
}

TEST_F(GamepadDeviceTest, DisconnectClearsState)
{
    // Connect and set axis value.
    RawInputFrame f1;
    f1.gamepadConnections.push_back({ 1000, true, 0 });
    f1.gamepadAxes.push_back({ 1000, GamepadAxis::LeftX, 0.9f, 0 });
    gp.BeginFrame(); gp.ConsumeFrame(f1);

    EXPECT_FLOAT_EQ(gp.GetState().gamepad.GetAxis(GamepadAxis::LeftX), 0.9f);

    // Disconnect.
    RawInputFrame f2;
    f2.gamepadConnections.push_back({ 1000, false, 1 });
    gp.BeginFrame(); gp.ConsumeFrame(f2);

    EXPECT_FALSE(gp.IsConnected());
    EXPECT_FLOAT_EQ(gp.GetState().gamepad.GetAxis(GamepadAxis::LeftX), 0.f);
}

TEST_F(GamepadDeviceTest, IgnoresEventsForOtherDeviceId)
{
    RawInputFrame frame;
    frame.gamepadConnections.push_back({ 1000, true, 0 });
    frame.gamepadButtons.push_back({ 9999, GamepadButton::South, true, 0 });

    gp.BeginFrame(); gp.ConsumeFrame(frame);

    EXPECT_FALSE(gp.GetState().gamepad.IsButtonDown(GamepadButton::South));
}

// ─── InputActionMap tests ─────────────────────────────────────────────────────

class InputActionMapTest : public ::testing::Test
{
protected:
    InputActionMap map{ "test" };

    InputState MakeKeyState(KeyCode key, bool isDown, bool wasDown = false)
    {
        InputState state;
        if (wasDown) state.keyboard.SetDown(key, true);
        state.keyboard.FlipFrame();
        state.keyboard.SetDown(key, isDown);
        return state;
    }
};

TEST_F(InputActionMapTest, BindAndResolvePressed)
{
    map.Bind(A(Action::Jump), KeyChord{ KeyCode::Space });

    // Key just pressed (current=down, prev=up).
    const InputState state = MakeKeyState(KeyCode::Space, /*isDown=*/true, /*wasDown=*/false);

    std::vector<InputActionEvent> events;
    map.Resolve(state, /*tick=*/1, events);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].id,    A(Action::Jump));
    EXPECT_EQ(events[0].phase, ActionPhase::Pressed);
    EXPECT_FLOAT_EQ(events[0].value, 1.f);
    EXPECT_EQ(events[0].tick, 1u);
}

TEST_F(InputActionMapTest, ResolveHeld)
{
    map.Bind(A(Action::MoveForward), KeyChord{ KeyCode::W });

    const InputState state = MakeKeyState(KeyCode::W, true, true);

    std::vector<InputActionEvent> events;
    map.Resolve(state, 2, events);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].phase, ActionPhase::Held);
}

TEST_F(InputActionMapTest, ResolveReleased)
{
    map.Bind(A(Action::Jump), KeyChord{ KeyCode::Space });

    const InputState state = MakeKeyState(KeyCode::Space, /*isDown=*/false, /*wasDown=*/true);

    std::vector<InputActionEvent> events;
    map.Resolve(state, 3, events);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].phase, ActionPhase::Released);
    EXPECT_FLOAT_EQ(events[0].value, 0.f);
}

TEST_F(InputActionMapTest, NoEventWhenKeyNeverTouched)
{
    map.Bind(A(Action::Jump), KeyChord{ KeyCode::Space });

    InputState state;  // all keys up, prev also up
    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    EXPECT_TRUE(events.empty());
}

TEST_F(InputActionMapTest, MouseButtonBinding)
{
    map.Bind(A(Action::Fire), MouseButtonChord{ MouseButton::Left });

    InputState state;
    state.mouse.SetButtonDown(MouseButton::Left, true);
    // prev is not set → WasButtonJustPressed = true

    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].id,    A(Action::Fire));
    EXPECT_EQ(events[0].phase, ActionPhase::Pressed);
}

TEST_F(InputActionMapTest, GamepadAxisBinding)
{
    map.Bind(A(Action::MoveForward),
             GamepadAxisChord{ GamepadAxis::LeftY, /*threshold=*/0.2f, /*direction=*/-1.f });

    InputState state;
    state.gamepad.connected = true;
    // Left stick pushed forward (negative Y in our convention).
    state.gamepad.SetAxis(GamepadAxis::LeftY, -0.8f);

    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].phase, ActionPhase::Analog);
    EXPECT_GT(events[0].value, 0.f);
    EXPECT_LE(events[0].value, 1.f);
}

TEST_F(InputActionMapTest, GamepadAxisBelowThresholdNoEvent)
{
    map.Bind(A(Action::MoveForward),
             GamepadAxisChord{ GamepadAxis::LeftY, 0.2f, -1.f });

    InputState state;
    state.gamepad.connected = true;
    state.gamepad.SetAxis(GamepadAxis::LeftY, -0.1f);  // below threshold

    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    EXPECT_TRUE(events.empty());
}

TEST_F(InputActionMapTest, UnbindRemovesAction)
{
    map.Bind(A(Action::Jump), KeyChord{ KeyCode::Space });
    map.Unbind(A(Action::Jump));

    const InputState state = MakeKeyState(KeyCode::Space, true, false);
    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    EXPECT_TRUE(events.empty());
}

TEST_F(InputActionMapTest, MultipleBindingsSameAction)
{
    // Both Space (keyboard) and South (gamepad) → Jump.
    map.Bind(A(Action::Jump), KeyChord{ KeyCode::Space });
    map.Bind(A(Action::Jump), GamepadButtonChord{ GamepadButton::South });

    // Press Space only.
    InputState state = MakeKeyState(KeyCode::Space, true, false);
    state.gamepad.connected = true;

    std::vector<InputActionEvent> events;
    map.Resolve(state, 1, events);

    // Both bindings are checked → but only the key fires (gamepad button not pressed).
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].id, A(Action::Jump));
}

// ─── InputActionMapStack tests ────────────────────────────────────────────────

TEST(InputActionMapStackTest, HigherPriorityMapResolvesFirst)
{
    auto baseMap   = std::make_shared<InputActionMap>("base");
    auto vehicleMap= std::make_shared<InputActionMap>("vehicle");

    baseMap->Bind(A(Action::Jump), KeyChord{ KeyCode::Space });
    vehicleMap->Bind(A(Action::Jump), KeyChord{ KeyCode::Space });

    InputActionMapStack stack;
    stack.Push(baseMap);
    stack.Push(vehicleMap);  // higher priority

    InputState state;
    state.keyboard.SetDown(KeyCode::Space, true);  // no flip, all "fresh"

    const auto events = stack.ResolveAll(state, 1);

    // Both maps fire (same action, not consumed in this simple case).
    // But neither should crash and all events should have correct action ID.
    for (const auto& ev : events)
        EXPECT_EQ(ev.id, A(Action::Jump));
}

TEST(InputActionMapStackTest, RemoveByName)
{
    auto map1 = std::make_shared<InputActionMap>("alpha");
    auto map2 = std::make_shared<InputActionMap>("beta");

    InputActionMapStack stack;
    stack.Push(map1);
    stack.Push(map2);
    stack.Remove("alpha");

    // map2 still there.
    InputState state;
    // No assertions on event content — just verify no crash.
    const auto events = stack.ResolveAll(state, 1);
    (void)events;
}

// ─── DeviceRegistry tests ────────────────────────────────────────────────────

TEST(DeviceRegistryTest, RegisterAndFind)
{
    DeviceRegistry reg;
    reg.RegisterDevice(MakeKeyboardDevice(1));
    reg.RegisterDevice(MakeMouseDevice(2));

    EXPECT_NE(reg.FindDevice(1), nullptr);
    EXPECT_NE(reg.FindDevice(2), nullptr);
    EXPECT_EQ(reg.FindDevice(99), nullptr);
    EXPECT_EQ(reg.GetDeviceCount(), 2u);
}

TEST(DeviceRegistryTest, UnregisterRemovesDevice)
{
    DeviceRegistry reg;
    reg.RegisterDevice(MakeKeyboardDevice(1));
    reg.UnregisterDevice(1);

    EXPECT_EQ(reg.FindDevice(1), nullptr);
    EXPECT_EQ(reg.GetDeviceCount(), 0u);
}

TEST(DeviceRegistryTest, DeviceEventCallbackFires)
{
    DeviceRegistry reg;
    std::vector<std::tuple<DeviceId, DeviceType, bool>> events;

    reg.SetDeviceEventCallback([&](DeviceId id, DeviceType dt, bool connected)
    {
        events.emplace_back(id, dt, connected);
    });

    reg.RegisterDevice(MakeKeyboardDevice(7));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(std::get<0>(events[0]), 7u);
    EXPECT_EQ(std::get<2>(events[0]), true);

    reg.UnregisterDevice(7);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(std::get<2>(events[1]), false);
}

TEST(DeviceRegistryTest, GamepadHotplugViaConnectionEvents)
{
    DeviceRegistry reg;

    RawInputFrame f;
    f.gamepadConnections.push_back({ 1000, true, 0 });

    reg.ProcessConnectionEvents(f);
    EXPECT_NE(reg.FindDevice(1000), nullptr);

    RawInputFrame f2;
    f2.gamepadConnections.push_back({ 1000, false, 1 });
    reg.ProcessConnectionEvents(f2);
    EXPECT_EQ(reg.FindDevice(1000), nullptr);
}

TEST(DeviceRegistryTest, GetDevicesOfType)
{
    DeviceRegistry reg;
    reg.RegisterDevice(MakeKeyboardDevice(1));
    reg.RegisterDevice(MakeKeyboardDevice(2));
    reg.RegisterDevice(MakeMouseDevice(3));

    EXPECT_EQ(reg.GetDevicesOfType(DeviceType::Keyboard).size(), 2u);
    EXPECT_EQ(reg.GetDevicesOfType(DeviceType::Mouse).size(),    1u);
    EXPECT_EQ(reg.GetDevicesOfType(DeviceType::Gamepad).size(),  0u);
}

// ─── GameplayInputFrame query helpers ────────────────────────────────────────

TEST(GameplayInputFrameTest, WasPressedQuery)
{
    GameplayInputFrame frame;
    frame.actions.push_back({ A(Action::Jump), ActionPhase::Pressed, 1.f, 5 });
    frame.actions.push_back({ A(Action::Fire), ActionPhase::Held,    1.f, 5 });

    EXPECT_TRUE (frame.WasPressed(A(Action::Jump)));
    EXPECT_FALSE(frame.WasPressed(A(Action::Fire)));   // Held, not Pressed
    EXPECT_FALSE(frame.WasPressed(A(Action::Sprint)));  // not in frame
}

TEST(GameplayInputFrameTest, IsHeldQuery)
{
    GameplayInputFrame frame;
    frame.actions.push_back({ A(Action::Fire), ActionPhase::Held, 1.f, 5 });

    EXPECT_TRUE(frame.IsHeld(A(Action::Fire)));
}

TEST(GameplayInputFrameTest, WasReleasedQuery)
{
    GameplayInputFrame frame;
    frame.actions.push_back({ A(Action::Jump), ActionPhase::Released, 0.f, 5 });

    EXPECT_TRUE(frame.WasReleased(A(Action::Jump)));
}

TEST(GameplayInputFrameTest, GetValueQuery)
{
    GameplayInputFrame frame;
    frame.actions.push_back({ A(Action::MoveForward), ActionPhase::Analog, 0.75f, 5 });

    EXPECT_FLOAT_EQ(frame.GetValue(A(Action::MoveForward)), 0.75f);
    EXPECT_FLOAT_EQ(frame.GetValue(A(Action::Jump)), 0.f);  // not present
}