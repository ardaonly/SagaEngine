#pragma once
#include "SagaEditor/Collaboration/Session/CollaborationSessionState.h"
#include "SagaEditor/Collaboration/Session/SessionId.h"
#include <functional>
namespace SagaEditor::Collaboration {
struct SessionConfig;
class ICollaborationSession {
public:
    virtual ~ICollaborationSession() = default;
    virtual bool Open(const SessionConfig& cfg) = 0;
    virtual void Close()                        = 0;
    virtual CollaborationSessionState GetState() const noexcept = 0;
    virtual SessionId GetSessionId()             const noexcept = 0;
    virtual void SetOnStateChanged(std::function<void(CollaborationSessionState)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
