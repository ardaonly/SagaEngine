#include "SagaEngine/Input/Devices/IInputDevice.h"
#include "SagaEngine/Input/Events/InputEvent.h"
#include <atomic>

namespace Saga::Input {

class GamepadDevice final : public IInputDevice {
public:
    GamepadDevice(DeviceId id, std::string name);
    ~GamepadDevice() override = default;
    DeviceId GetId() const override;
    std::string GetName() const override;
    void Update() override;
    bool IsConnected() const override;
    void SetEventCallback(EventCallback cb) override;
    void SimulateButton(int button, bool pressed);
    void SimulateAxis(int axisId, float value);
private:
    DeviceId m_id;
    std::string m_name;
    EventCallback m_callback;
    std::atomic<bool> m_connected;
};

GamepadDevice::GamepadDevice(DeviceId id, std::string name)
: m_id(id)
, m_name(std::move(name))
, m_callback(nullptr)
, m_connected(true)
{
}

DeviceId GamepadDevice::GetId() const {
    return m_id;
}

std::string GamepadDevice::GetName() const {
    return m_name;
}

void GamepadDevice::Update() {
}

bool GamepadDevice::IsConnected() const {
    return m_connected.load();
}

void GamepadDevice::SetEventCallback(EventCallback cb) {
    m_callback = std::move(cb);
}

void GamepadDevice::SimulateButton(int button, bool pressed) {
    if (!m_callback) return;
    InputEvent e;
    e.type = pressed ? InputEventType::GamepadButtonDown : InputEventType::GamepadButtonUp;
    e.device = m_id;
    e.key = button;
    e.pressed = pressed;
    m_callback(e);
}

void GamepadDevice::SimulateAxis(int axisId, float value) {
    if (!m_callback) return;
    InputEvent e;
    e.type = InputEventType::Axis;
    e.device = m_id;
    e.axisId = axisId;
    e.axisValue = value;
    m_callback(e);
}

}
