#include "SagaEngine/Input/Devices/IInputDevice.h"
#include "SagaEngine/Input/Events/InputEvent.h"
#include <atomic>

namespace Saga::Input {

class MouseDevice final : public IInputDevice {
public:
    MouseDevice(DeviceId id, std::string name);
    ~MouseDevice() override = default;
    DeviceId GetId() const override;
    std::string GetName() const override;
    void Update() override;
    bool IsConnected() const override;
    void SetEventCallback(EventCallback cb) override;
    void SimulateMove(float x, float y);
    void SimulateButton(int button, bool pressed);
private:
    DeviceId m_id;
    std::string m_name;
    EventCallback m_callback;
    std::atomic<bool> m_connected;
};

MouseDevice::MouseDevice(DeviceId id, std::string name)
: m_id(id)
, m_name(std::move(name))
, m_callback(nullptr)
, m_connected(true)
{
}

DeviceId MouseDevice::GetId() const {
    return m_id;
}

std::string MouseDevice::GetName() const {
    return m_name;
}

void MouseDevice::Update() {
}

bool MouseDevice::IsConnected() const {
    return m_connected.load();
}

void MouseDevice::SetEventCallback(EventCallback cb) {
    m_callback = std::move(cb);
}

void MouseDevice::SimulateMove(float x, float y) {
    if (!m_callback) return;
    InputEvent e;
    e.type = InputEventType::MouseMove;
    e.device = m_id;
    e.mouseX = x;
    e.mouseY = y;
    m_callback(e);
}

void MouseDevice::SimulateButton(int button, bool pressed) {
    if (!m_callback) return;
    InputEvent e;
    e.type = pressed ? InputEventType::MouseButtonDown : InputEventType::MouseButtonUp;
    e.device = m_id;
    e.mouseButton = button;
    e.pressed = pressed;
    m_callback(e);
}

}
