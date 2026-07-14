#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct CollaborationClientConfig {
    std::string serverAddress = "localhost";
    uint16_t    serverPort    = 7777;
    std::string authToken;
    uint32_t    reconnectIntervalMs = 3000;
};
} // namespace SagaEditor::Collaboration
