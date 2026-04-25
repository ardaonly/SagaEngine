#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct SessionConfig {
    std::string projectId;
    std::string sceneId;
    uint16_t    maxParticipants = 8;
    bool        allowObservers  = true;
};
} // namespace SagaEditor::Collaboration
