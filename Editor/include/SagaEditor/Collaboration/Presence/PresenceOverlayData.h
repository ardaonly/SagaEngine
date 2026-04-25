#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor::Collaboration {
struct PresenceOverlayEntry {
    uint64_t    userId = 0;
    std::string username;
    float       x = 0.f, y = 0.f;
    uint32_t    color = 0xFFFFFFFF; ///< RGBA
};
using PresenceOverlayData = std::vector<PresenceOverlayEntry>;
} // namespace SagaEditor::Collaboration
