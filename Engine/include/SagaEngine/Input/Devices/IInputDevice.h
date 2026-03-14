#pragma once
#include "../Events/InputEvent.h"
#include <functional>
#include <string>

namespace Saga::Input {

using EventCallback = std::function<void(const InputEvent&)>;

class IInputDevice {
public:
    virtual ~IInputDevice() = default;
    virtual DeviceId GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual void Update() = 0;
    virtual bool IsConnected() const = 0;
    virtual void SetEventCallback(EventCallback cb) = 0;
};

}
