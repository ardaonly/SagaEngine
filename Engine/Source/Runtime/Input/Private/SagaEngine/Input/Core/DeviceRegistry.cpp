/// @file DeviceRegistry.cpp
/// @brief Implements device registry ownership, dispatch, and hotplug routing.

#include "SagaEngine/Input/Core/DeviceRegistry.h"
#include "SagaEngine/Input/Devices/GamepadDevice.h"

namespace SagaEngine::Input
{

void DeviceRegistry::SetDeviceEventCallback(DeviceEventCallback cb)
{
    m_onDeviceEvent = std::move(cb);
}

size_t DeviceRegistry::GetDeviceCount() const noexcept
{
    return m_devices.size();
}

void DeviceRegistry::RegisterDevice(std::unique_ptr<InputDevice> device)
{
    const DeviceId id   = device->GetId();
    const DeviceType dt = device->GetType();
    m_devices[id]       = std::move(device);

    if (m_onDeviceEvent)
        m_onDeviceEvent(id, dt, /*connected=*/true);
}

void DeviceRegistry::UnregisterDevice(DeviceId id)
{
    auto it = m_devices.find(id);
    if (it == m_devices.end()) return;

    const DeviceType dt = it->second->GetType();
    m_devices.erase(it);

    if (m_onDeviceEvent)
        m_onDeviceEvent(id, dt, /*connected=*/false);
}

InputDevice* DeviceRegistry::FindDevice(DeviceId id) noexcept
{
    auto it = m_devices.find(id);
    return it != m_devices.end() ? it->second.get() : nullptr;
}

const InputDevice* DeviceRegistry::FindDevice(DeviceId id) const noexcept
{
    auto it = m_devices.find(id);
    return it != m_devices.end() ? it->second.get() : nullptr;
}

std::vector<InputDevice*> DeviceRegistry::GetDevicesOfType(DeviceType type)
{
    std::vector<InputDevice*> out;
    for (auto& [id, dev] : m_devices)
        if (dev->GetType() == type)
            out.push_back(dev.get());
    return out;
}

void DeviceRegistry::BeginFrame() noexcept
{
    for (auto& [id, dev] : m_devices)
        dev->BeginFrame();
}

void DeviceRegistry::ConsumeFrame(const RawInputFrame& frame)
{
    for (auto& [id, dev] : m_devices)
        dev->ConsumeFrame(frame);
}

void DeviceRegistry::ProcessConnectionEvents(const RawInputFrame& frame)
{
    for (const auto& ev : frame.gamepadConnections)
    {
        if (ev.connected)
        {
            // Only create a new device if one doesn't already exist for this ID.
            if (!FindDevice(ev.deviceId))
                RegisterDevice(MakeGamepadDevice(ev.deviceId));
        }
        else
        {
            UnregisterDevice(ev.deviceId);
        }
    }
}

} // namespace SagaEngine::Input