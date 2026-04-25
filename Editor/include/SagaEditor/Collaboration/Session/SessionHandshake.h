#pragma once
#include "SagaEditor/Collaboration/Session/CollaborationToken.h"
#include "SagaEditor/Collaboration/Session/SessionId.h"
namespace SagaEditor::Collaboration {
class SessionHandshake {
public:
    struct Result { bool success; SessionId sessionId; CollaborationToken token; };
    Result Perform(const std::string& serverAddress, uint16_t port,
                   const std::string& projectId, const std::string& authToken);
};
} // namespace SagaEditor::Collaboration
