#include "SagaEngine/Input/InputManager.h"

#include <algorithm>

namespace Saga::Input {

InputManager& InputManager::Instance() {
    static InputManager instance;
    return instance;
}

InputManager::InputManager()
: m_nextSubscriberId(1)
{
}

void InputManager::RegisterDevice(std::unique_ptr<IInputDevice> device) {
    std::lock_guard<std::mutex> lock(m_mutex);
    device->SetEventCallback([this](const InputEvent& e){ this->DispatchEvent(e); });
    m_devices.push_back(std::move(device));
}

void InputManager::UnregisterDevice(DeviceId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_devices.begin(), m_devices.end(), [id](const std::unique_ptr<IInputDevice>& d){
        return d->GetId() == id;
    });
    m_devices.erase(it, m_devices.end());
}

SubscriberId InputManager::Subscribe(EventCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SubscriberId id = m_nextSubscriberId++;
    m_subscribers.emplace(id, std::move(cb));
    return id;
}

void InputManager::Unsubscribe(SubscriberId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers.erase(id);
}

void InputManager::PushMappingContext(const MappingContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mappingStack.push_back(ctx);
    std::sort(m_mappingStack.begin(), m_mappingStack.end(), [](const MappingContext& a, const MappingContext& b){
        return a.priority > b.priority;
    });
}

void InputManager::PopMappingContext(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_mappingStack.begin(), m_mappingStack.end(), [&name](const MappingContext& c){
        return c.name == name;
    });
    m_mappingStack.erase(it, m_mappingStack.end());
}

bool InputManager::IsKeyDown(int key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_keyState.find(key);
    return it != m_keyState.end() && it->second;
}

bool InputManager::WasKeyPressedThisFrame(int key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_keyPressedThisFrame.find(key);
    return it != m_keyPressedThisFrame.end() && it->second;
}

float InputManager::GetAxis(int axisId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_axisState.find(axisId);
    return it != m_axisState.end() ? it->second : 0.0f;
}

void InputManager::DispatchEvent(const InputEvent& e) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventQueue.push_back(e);
}

void InputManager::ApplyMappings(const InputEvent& e) {
    for (const auto& ctx : m_mappingStack) {
        for (const auto& b : ctx.bindings) {
            if (b.sourceType == e.type) {
                if ((e.type == InputEventType::KeyDown || e.type == InputEventType::KeyUp || e.type == InputEventType::KeyRepeat)
                    && b.code == e.key) {
                    InputEvent mapped = e;
                    for (const auto& sub : m_subscribers) {
                        sub.second(mapped);
                        if (mapped.handled) return;
                    }
                }
                if ((e.type == InputEventType::MouseButtonDown || e.type == InputEventType::MouseButtonUp) && b.code == e.mouseButton) {
                    InputEvent mapped = e;
                    for (const auto& sub : m_subscribers) {
                        sub.second(mapped);
                        if (mapped.handled) return;
                    }
                }
                if (e.type == InputEventType::Axis && b.code == e.axisId) {
                    InputEvent mapped = e;
                    mapped.axisValue *= b.scale;
                    for (const auto& sub : m_subscribers) {
                        sub.second(mapped);
                        if (mapped.handled) return;
                    }
                }
            }
        }
    }
}

void InputManager::AdvanceFrame() {
    m_keyPressedThisFrame.clear();
}

void InputManager::Update(float dt) {
    std::vector<InputEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        events.swap(m_eventQueue);
    }
    for (auto& d : m_devices) {
        d->Update();
    }
    for (const auto& e : events) {
        if (e.type == InputEventType::KeyDown) {
            m_keyState[e.key] = true;
            m_keyPressedThisFrame[e.key] = true;
        } else if (e.type == InputEventType::KeyUp) {
            m_keyState[e.key] = false;
        } else if (e.type == InputEventType::Axis) {
            m_axisState[e.axisId] = e.axisValue;
        } else if (e.type == InputEventType::MouseMove) {
            // mouse position handled by subscribers
        } else if (e.type == InputEventType::MouseButtonDown) {
            m_keyState[100000 + e.mouseButton] = true;
            m_keyPressedThisFrame[100000 + e.mouseButton] = true;
        } else if (e.type == InputEventType::MouseButtonUp) {
            m_keyState[100000 + e.mouseButton] = false;
        }
        ApplyMappings(e);
        for (const auto& sub : m_subscribers) {
            sub.second(e);
            if (e.handled) break;
        }
    }
    AdvanceFrame();
}

} // namespace Saga::Input
