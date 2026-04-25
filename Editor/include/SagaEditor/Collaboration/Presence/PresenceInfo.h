#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct PresenceInfo {
    uint64_t    userId = 0;
    std::string username;
    std::string currentSceneId;
    float       cursorX = 0.f, cursorY = 0.f;
    bool        isActive = false;
};
} // namespace SagaEditor::Collaboration
