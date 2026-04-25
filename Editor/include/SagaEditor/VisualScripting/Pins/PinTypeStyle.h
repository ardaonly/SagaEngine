#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::VisualScripting {
struct PinStyle {
    uint32_t    color       = 0xFFFFFFFF; ///< RGBA
    std::string shape       = "circle";   ///< circle | square | diamond
    float       radius      = 6.f;
};
class PinTypeStyle {
public:
    static PinStyle ForType(const std::string& pinType);
    static void     Register(const std::string& pinType, PinStyle style);
};
} // namespace SagaEditor::VisualScripting
