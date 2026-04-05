/// @file DeviceRegistry.h
/// @brief Owns logical input devices and routes hotplug and per-frame input to them.
///
/// Layer  : Input / Core
/// Purpose: Stores all active InputDevice instances, dispatches RawInputFrame
///          data, and emits connection events when devices are added or removed.
///
/// Rules  :
///   - No platform headers included here.
///   - No direct backend ownership.
///   - All ownership of devices is unique_ptr-based.

#pragma once

#include "SagaEngine/Input/Devices/InputDevice.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Input
{

/// Callback fired when a device is registered or unregistered.
/// bool = true for connect, false for disconnect.
using DeviceEventCallback = std::function<void(DeviceId, DeviceType, bool)>;

/// Registry of active input devices.
///
/// Owns keyboard, mouse, and gamepad devices and forwards raw frame data to them.
class DeviceRegistry
{
public:
    DeviceRegistry() = default;
    ~DeviceRegistry() = default;

    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    /// Register a new device and emit a connect event.
    void RegisterDevice(std::unique_ptr<InputDevice> device);

    /// Unregister a device by id and emit a disconnect event.
    void UnregisterDevice(DeviceId id);

    /// Find a device by id.
    [[nodiscard]] InputDevice* FindDevice(DeviceId id) noexcept;
    [[nodiscard]] const InputDevice* FindDevice(DeviceId id) const noexcept;

    /// Return all devices of the requested type.
    [[nodiscard]] std::vector<InputDevice*> GetDevicesOfType(DeviceType type);

    /// Return the number of currently registered devices.
    [[nodiscard]] size_t GetDeviceCount() const noexcept;

    /// Set the callback fired on device connect/disconnect.
    void SetDeviceEventCallback(DeviceEventCallback cb);

    /// Advance all devices to the next frame.
    void BeginFrame() noexcept;

    /// Dispatch raw frame data to all registered devices.
    void ConsumeFrame(const RawInputFrame& frame);

    /// Process hotplug connection events from the raw frame.
    void ProcessConnectionEvents(const RawInputFrame& frame);

private:
    std::unordered_map<DeviceId, std::unique_ptr<InputDevice>> m_devices;
    DeviceEventCallback m_onDeviceEvent;
};

} // namespace SagaEngine::Input