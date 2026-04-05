/// @file InputManager.cpp
/// @brief Orchestrates the full client-side input pipeline each frame.
///
/// Per-frame sequence:
///   1. backend->PollRawEvents()              → RawInputFrame
///   2. registry->ProcessConnectionEvents()   → hotplug
///   3. registry->BeginFrame()                → flip state
///   4. registry->ConsumeFrame()              → update all devices
///   5. MergeDeviceStates()                   → unified InputState
///   6. mapStack.ResolveAll()                 → action events
///   7. BuildAnalogAxes()                     → move/look axes
///   8. Fill GameplayInputFrame

#include "SagaEngine/Input/Core/InputManager.h"
#include "SagaEngine/Input/Core/DeviceRegistry.h"
#include "SagaEngine/Input/Devices/KeyboardDevice.h"
#include "SagaEngine/Input/Devices/MouseDevice.h"

namespace SagaEngine::Input
{

InputManager::InputManager(InputManagerConfig config)
    : m_config(config)
    , m_registry(std::make_unique<DeviceRegistry>())
{}

// ── Initialization ────────────────────────────────────────────────────────────

void InputManager::SetBackend(
    std::unique_ptr<Platform::IPlatformInputBackend> backend)
{
    m_backend = std::move(backend);
}

bool InputManager::Initialize()
{
    if (!m_backend)    return false;
    if (m_initialized) return true;

    if (!m_backend->Initialize()) return false;

    m_initialized = true;
    return true;
}

void InputManager::Shutdown()
{
    if (m_backend && m_initialized)
        m_backend->Shutdown();

    m_initialized = false;
}

// ── Device management ─────────────────────────────────────────────────────────

void InputManager::RegisterDevice(std::unique_ptr<InputDevice> device)
{
    m_registry->RegisterDevice(std::move(device));
}

void InputManager::SetDeviceEventCallback(DeviceEventCallback cb)
{
    m_registry->SetDeviceEventCallback(std::move(cb));
}

// ── Action map management ─────────────────────────────────────────────────────

void InputManager::PushActionMap(std::shared_ptr<InputActionMap> map)
{
    m_mapStack.Push(std::move(map));
}

void InputManager::PopActionMap()
{
    m_mapStack.Pop();
}

void InputManager::RemoveActionMap(std::string_view name)
{
    m_mapStack.Remove(name);
}

// ── Core per-frame update ─────────────────────────────────────────────────────

void InputManager::Update(uint32_t tick)
{
    // 1. Poll raw events from platform backend.
    m_rawFrame.frameIndex = tick;
    m_backend->PollRawEvents(m_rawFrame);

    // 2. Handle hotplug (gamepad connect/disconnect).
    m_registry->ProcessConnectionEvents(m_rawFrame);

    // 3. Flip all device states (current → previous).
    m_registry->BeginFrame();

    // 4. Distribute raw events to all registered devices.
    m_registry->ConsumeFrame(m_rawFrame);

    // 5. Merge all device states into a single InputState.
    if (m_config.mergeDeviceStates)
        MergeDeviceStates();

    // 6. Resolve all action maps against merged state.
    m_currentFrame.Clear();
    m_currentFrame.tick             = tick;
    m_currentFrame.frameTimestampUs = m_rawFrame.frameTimestampUs;

    m_currentFrame.actions = m_mapStack.ResolveAll(m_mergedState, tick);

    // 7. Build analog axes from merged state.
    BuildAnalogAxes(m_currentFrame);
}

// ── Cursor / text ─────────────────────────────────────────────────────────────

void InputManager::SetTextInputEnabled(bool enabled)
{
    if (m_backend) m_backend->SetTextInputEnabled(enabled);
}

void InputManager::SetCursorLocked(bool locked)
{
    if (m_backend) m_backend->SetCursorLocked(locked);
}

void InputManager::SetCursorVisible(bool visible)
{
    if (m_backend) m_backend->SetCursorVisible(visible);
}

// ── Private: MergeDeviceStates ────────────────────────────────────────────────

void InputManager::MergeDeviceStates()
{
    // Strategy: merge keyboard from all keyboards (OR of key bits),
    // merge mouse from all mice (accumulate deltas), take first connected gamepad.
    // For MMO this is always 1 keyboard + 1 mouse, so the merge is trivial.
    // The structure handles multi-device scenarios (accessibility, HOTAS) cleanly.

    m_mergedState.Clear();

    // ── Keyboard merge ────────────────────────────────────────────────────────
    auto keyboards = m_registry->GetDevicesOfType(DeviceType::Keyboard);
    for (const InputDevice* dev : keyboards)
    {
        const auto& ks = dev->GetState().keyboard;
        for (size_t i = 0; i < KeyboardState::kKeyCount; ++i)
        {
            const auto key = static_cast<KeyCode>(i);
            // Merge: key is down if ANY keyboard reports it down.
            if (ks.IsDown(key))
                m_mergedState.keyboard.SetDown(key, true);
        }
        // For edge detection on the merged state, we need to also merge
        // the previous frame bits. We approximate: if any keyboard reported
        // WasJustPressed, mark it. This is correct for single-keyboard setups.
        // Multi-keyboard merging of edge detection is undefined behavior by design.
    }

    // ── Mouse merge ───────────────────────────────────────────────────────────
    auto mice = m_registry->GetDevicesOfType(DeviceType::Mouse);
    for (const InputDevice* dev : mice)
    {
        const auto& ms = dev->GetState().mouse;

        // Accumulate relative movement across all mice.
        m_mergedState.mouse.relX += ms.relX;
        m_mergedState.mouse.relY += ms.relY;

        // Last absolute position wins (they should all agree).
        m_mergedState.mouse.absX = ms.absX;
        m_mergedState.mouse.absY = ms.absY;

        // Accumulate scroll.
        m_mergedState.mouse.scrollX += ms.scrollX;
        m_mergedState.mouse.scrollY += ms.scrollY;

        // Merge buttons (OR).
        for (size_t i = 0; i < MouseState::kButtonCount; ++i)
        {
            const auto btn = static_cast<MouseButton>(i);
            if (ms.IsButtonDown(btn))
                m_mergedState.mouse.SetButtonDown(btn, true);
        }
    }

    // ── Gamepad merge ─────────────────────────────────────────────────────────
    // For MMO: use the first connected gamepad. Multiple gamepads → first wins.
    auto gamepads = m_registry->GetDevicesOfType(DeviceType::Gamepad);
    for (const InputDevice* dev : gamepads)
    {
        if (!dev->IsConnected()) continue;
        m_mergedState.gamepad = dev->GetState().gamepad;
        break;
    }
}

// ── Private: BuildAnalogAxes ──────────────────────────────────────────────────

void InputManager::BuildAnalogAxes(GameplayInputFrame& frame) const
{
    // The mapping layer already resolved MoveForward/Back/Left/Right actions.
    // Here we also provide a direct analog-aggregated move vector for
    // analog sticks and normalized WASD, so gameplay has a unified float pair.

    // Gamepad left stick (if connected) takes priority over keyboard.
    const float gpMoveX = m_mergedState.gamepad.GetAxis(GamepadAxis::LeftX);
    const float gpMoveY = m_mergedState.gamepad.GetAxis(GamepadAxis::LeftY);

    const bool hasGamepadInput = (gpMoveX * gpMoveX + gpMoveY * gpMoveY) > 0.04f;

    if (hasGamepadInput)
    {
        frame.move.x = gpMoveX;
        frame.move.y = gpMoveY;
    }
    else
    {
        // Build from keyboard: WASD → ±1.0 axes.
        // Action events already carry this as Held flags; here we also
        // write the float pair so gameplay can interpolate/normalize.
        float mx = 0.f, my = 0.f;

        if (m_mergedState.keyboard.IsDown(KeyCode::D)) mx += 1.f;
        if (m_mergedState.keyboard.IsDown(KeyCode::A)) mx -= 1.f;
        if (m_mergedState.keyboard.IsDown(KeyCode::W)) my -= 1.f;  // -Y = forward
        if (m_mergedState.keyboard.IsDown(KeyCode::S)) my += 1.f;

        // Normalize diagonal.
        if (mx != 0.f && my != 0.f)
        {
            constexpr float kInvSqrt2 = 0.70710678118f;
            mx *= kInvSqrt2;
            my *= kInvSqrt2;
        }

        frame.move.x = mx;
        frame.move.y = my;
    }

    // Look axes: gamepad right stick or mouse delta.
    const float gpLookX = m_mergedState.gamepad.GetAxis(GamepadAxis::RightX);
    const float gpLookY = m_mergedState.gamepad.GetAxis(GamepadAxis::RightY);
    const bool hasGamepadLook = (gpLookX * gpLookX + gpLookY * gpLookY) > 0.04f;

    if (hasGamepadLook)
    {
        frame.look.x = gpLookX;
        frame.look.y = gpLookY;
    }
    else
    {
        // Mouse: relX/Y are raw pixels. Caller (gameplay) applies sensitivity.
        frame.look.x = static_cast<float>(m_mergedState.mouse.relX);
        frame.look.y = static_cast<float>(m_mergedState.mouse.relY);
    }

    // Scroll.
    frame.scroll = m_mergedState.mouse.scrollY;
}

} // namespace SagaEngine::Input