#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct CollaborationServerState {
    bool     isRunning     = false;
    uint16_t port          = 0;
    uint32_t clientCount   = 0;
    std::string sessionId;
};
} // namespace SagaEditor::Collaboration
