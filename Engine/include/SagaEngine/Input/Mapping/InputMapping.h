#pragma once
#include "../Events/InputTypes.h"
#include <string>
#include <vector>

namespace Saga::Input {

struct Binding {
    std::string actionName;
    InputEventType sourceType;
    int code;
    float scale;
    Binding() = default;
    Binding(std::string a, InputEventType s, int c, float sc = 1.0f);
};

struct MappingContext {
    std::string name;
    int priority;
    std::vector<Binding> bindings;
    MappingContext() = default;
    MappingContext(std::string n, int p = 0);
};

}
