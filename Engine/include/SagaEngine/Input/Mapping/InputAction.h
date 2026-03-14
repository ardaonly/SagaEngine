#pragma once
#include "../Events/InputEvent.h"
#include <functional>
#include <string>

namespace Saga::Input {

using ActionCallback = std::function<void(const InputEvent&)>;

struct InputAction {
    std::string name;
    ActionCallback callback;
    InputAction() = default;
    InputAction(std::string n, ActionCallback cb);
};

}
