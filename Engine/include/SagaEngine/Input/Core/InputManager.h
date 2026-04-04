#pragma once

/// @file InputManager.h
/// @brief Orchestrates the full input pipeline each frame.
///
/// Layer  : Input (top of client-side input domain)
/// Purpose: Single entry point for the input system.
///
///   Each frame, InputManager:
///     1. Calls backend->PollRawEvents()           → RawInputFrame
///     2. Calls registry->ProcessConnectionEvents() → handles hotplug
///     3. Calls registry->BeginFrame()             → flip state
///     4. Calls registry->ConsumeFrame()           → update all devices
///     5. Aggregates InputState across devices      → merged state
///     6. Calls actionMapStack->ResolveAll()       → action events
///     7. Fills GameplayInputFrame for this tick
///
/// Dependencies:
///   InputManager → IPlatformInputBackend (owns via unique_ptr)
///   InputManager → DeviceRegistry        (owns via unique_ptr)
///   InputManager → InputActionMapStack   (owns)
///   InputManager → GameplayInputFrame    (produces)
///
///   InputManager does NOT depend on:
///   - SDL (or any platform backend directly)
///   - InputCommand / network
///   - Any gameplay simulation code
///
/// Thread safety:
///   Update() must be called from the input thread (main thread in most engines).
///   GetCurrentFrame() returns a const reference valid until next Update().
///   If the game thread reads the frame, protect with a frame copy or double buffer.

#include "SagaEngine/Input/Core/DeviceRegistry.h"
#include "SagaEngine/Input/Mapping/InputActionMap.h"
#include "SagaEngine/Input/Mapping/GameplayInputFrame.h"
#include "SagaEngine/Input/Backends/IPlatformInputBackend.h"
#include <memory>
#include <functional>

namespace SagaEngine::Input
{

struct InputManagerConfig
{
    /// Aggregated InputState across all devices is built by merging
    /// keyboard from device 0, mouse from device 1, etc. This flag
    /// enables that merge. Disable only if per-device state is needed.
    bool mergeDeviceStates = true;
};

class InputManager
{
public:
    explicit InputManager(InputManagerConfig config = {});
    ~InputManager() = default;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Initialization

    /// Attach the platform backend. Must be called before Update().
    /// InputManager takes ownership.
    void SetBackend(std::unique_ptr<Platform::IPlatformInputBackend> backend);

    /// Initialize the backend. Must be called after SetBackend().
    /// Returns false if the backend fails to initialize.
    [[nodiscard]] bool Initialize();

    /// Shutdown the backend and clear all state.
    void Shutdown();

    // Device management

    /// Manually register a device (e.g. keyboard, mouse).
    /// Gamepads are auto-registered from connection events.
    void RegisterDevice(std::unique_ptr<InputDevice> device);

    void SetDeviceEventCallback(DeviceEventCallback cb);

    // Action map management

    void PushActionMap(std::shared_ptr<InputActionMap> map);
    void PopActionMap();
    void RemoveActionMap(std::string_view name);

    // Per-frame update

    /// Core method. Must be called once per simulation tick.
    /// @param tick  Current simulation tick number (for stamping events).
    void Update(uint32_t tick);

    // Output

    /// Returns the GameplayInputFrame for the most recent tick.
    /// Valid until the next call to Update().
    [[nodiscard]] const GameplayInputFrame& GetCurrentFrame() const noexcept
    {
        return m_currentFrame;
    }

    /// Returns the raw state merged from all devices.
    /// Prefer GetCurrentFrame() in gameplay code.
    [[nodiscard]] const InputState& GetMergedState() const noexcept
    {
        return m_mergedState;
    }

    // Text input

    void SetTextInputEnabled(bool enabled);

    // Cursor

    void SetCursorLocked(bool locked);
    void SetCursorVisible(bool visible);

    // Diagnostics

    [[nodiscard]] bool IsBackendReady() const noexcept { return m_initialized; }

private:
    /// Merge all device states into m_mergedState.
    void MergeDeviceStates();

    /// Build the analog move/look axes from merged state or action events.
    void BuildAnalogAxes(GameplayInputFrame& frame) const;

    InputManagerConfig                                      m_config;
    std::unique_ptr<Platform::IPlatformInputBackend>        m_backend;
    std::unique_ptr<DeviceRegistry>                         m_registry;
    InputActionMapStack                                     m_mapStack;

    RawInputFrame                                           m_rawFrame;
    InputState                                              m_mergedState;
    GameplayInputFrame                                      m_currentFrame;

    bool m_initialized = false;
};

} // namespace SagaEngine::Input