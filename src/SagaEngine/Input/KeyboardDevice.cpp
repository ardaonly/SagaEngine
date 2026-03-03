#include "SagaEngine/Input/Devices/IInputDevice.h"
#include "SagaEngine/Input/Events/InputEvent.h"
#include <atomic>
#include <chrono>

namespace Saga::Input {

class KeyboardDevice final : public IInputDevice {
public:
    KeyboardDevice(DeviceId id, std::string name);
    ~KeyboardDevice() override = default;
    DeviceId GetId() const override;
    std::string GetName() const override;
    void Update() override;
    bool IsConnected() const override;
    void SetEventCallback(EventCallback cb) override;
    void SimulateKey(int key, bool pressed);
private:
    DeviceId m_id;
    std::string m_name;
    EventCallback m_callback;
    std::atomic<bool> m_connected;
};

KeyboardDevice::KeyboardDevice(DeviceId id, std::string name)
: m_id(id)
, m_name(std::move(name))
, m_callback(nullptr)
, m_connected(true)
{
}

DeviceId KeyboardDevice::GetId() const {
    return m_id;
}

std::string KeyboardDevice::GetName() const {
    return m_name;
}

void KeyboardDevice::Update() {
}

bool KeyboardDevice::IsConnected() const {
    return m_connected.load();
}

void KeyboardDevice::SetEventCallback(EventCallback cb) {
    m_callback = std::move(cb);
}

void KeyboardDevice::SimulateKey(int key, bool pressed) {
    if (!m_callback) return;
    InputEvent e;
    e.type = pressed ? InputEventType::KeyDown : InputEventType::KeyUp;
    e.device = m_id;
    e.key = key;
    e.pressed = pressed;
    m_callback(e);
}

}
