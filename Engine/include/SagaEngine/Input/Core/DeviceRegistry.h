#pragma once

/// @file DeviceRegistry.h
/// @brief Tracks the set of active InputDevices and handles hotplug events.
///
/// Layer  : Input / Devices
/// Purpose: InputManager delegates device ownership and lookup here.
///          DeviceRegistry reacts to RawGamepadConnectionEvents so
///          the rest of the system doesn't need to.
///
/// Ownership model:
///   DeviceRegistry owns all InputDevice instances via unique_ptr.
///   External code holds raw pointers or DeviceId references only.
///
/// Thread safety:
///   All methods must be called from the input thread.
///   If another thread needs device state, it reads the InputState
///   snapshot produced by InputManager::Update() — not the device directly.

#include "SagaEngine/Input/Devices/InputDevice.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Input
{

/// Callback signature for device connect / disconnect events.
using DeviceEventCallback = std::function<void(DeviceId id, DeviceType type, bool connected)>;

class DeviceRegistry
{
public:
    DeviceRegistry()  = default;
    ~DeviceRegistry() = default;

    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    // Registration

    /// Take ownership of a device. Fires the connected callback.
    void RegisterDevice(std::unique_ptr<InputDevice> device);

    /// Remove a device by ID. Fires the disconnected callback.
    void UnregisterDevice(DeviceId id);

    // Lookup

    [[nodiscard]] InputDevice*       FindDevice(DeviceId id) noexcept;
    [[nodiscard]] const InputDevice* FindDevice(DeviceId id) const noexcept;

    /// Returns all devices of a given type (keyboard, mouse, gamepad...).
    [[nodiscard]] std::vector<InputDevice*> GetDevicesOfType(DeviceType type);

    // Per-frame update

    /// BeginFrame on every registered device.
    void BeginFrame() noexcept;

    /// Distribute the RawInputFrame to all registered devices.
    void ConsumeFrame(const RawInputFrame& frame);

    /// Process gamepad connection/disconnection events from the raw frame.
    /// Auto-creates or removes GamepadDevice instances as needed.
    void ProcessConnectionEvents(const RawInputFrame& frame);

    // Callbacks

    void SetDeviceEventCallback(DeviceEventCallback cb) { m_onDeviceEvent = std::move(cb); }

    // Diagnostics

    [[nodiscard]] size_t GetDeviceCount() const noexcept { return m_devices.size(); }

private:
    std::unordered_map<DeviceId, std::unique_ptr<InputDevice>> m_devices;
    DeviceEventCallback m_onDeviceEvent;
    DeviceId            m_nextGamepadId = 1000;  ///< Stable IDs for gamepads
};

} // namespace SagaEngine::Input