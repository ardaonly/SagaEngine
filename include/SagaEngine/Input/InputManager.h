#pragma once

#include "Devices/IInputDevice.h"
#include "Events/InputEvent.h"
#include "Mapping/InputMapping.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Saga::Input {

using EventCallback = std::function<void(const InputEvent&)>;
using SubscriberId = uint32_t;
using DeviceId = ::Saga::Input::DeviceId;

class InputManager {
public:
    static InputManager& Instance();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void RegisterDevice(std::unique_ptr<IInputDevice> device);
    void UnregisterDevice(DeviceId id);

    SubscriberId Subscribe(EventCallback cb);
    void Unsubscribe(SubscriberId id);

    void PushMappingContext(const MappingContext& ctx);
    void PopMappingContext(const std::string& name);

    bool IsKeyDown(int key) const;
    bool WasKeyPressedThisFrame(int key) const;
    float GetAxis(int axisId) const;

    void Update(float dt);

private:
    InputManager();

    void DispatchEvent(const InputEvent& e);
    void ApplyMappings(const InputEvent& e);
    void AdvanceFrame();

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<IInputDevice>> m_devices;
    std::unordered_map<SubscriberId, EventCallback> m_subscribers;
    SubscriberId m_nextSubscriberId;
    std::vector<InputEvent> m_eventQueue;

    std::unordered_map<int, bool> m_keyState;
    std::unordered_map<int, bool> m_keyPressedThisFrame;
    std::unordered_map<int, float> m_axisState;

    std::vector<MappingContext> m_mappingStack;
};

} // namespace Saga::Input
