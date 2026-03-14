#pragma once
#include "InputTypes.h"
#include <cstdint>

namespace Saga::Input {

struct InputEvent {
    InputEventType type;
    DeviceId device;
    Timestamp timestamp;
    int key;
    bool pressed;
    int mouseButton;
    float mouseX;
    float mouseY;
    int axisId;
    float axisValue;
    bool handled;
    InputEvent();
};

}
