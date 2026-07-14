#pragma once
#include "SagaEditor/Collaboration/Session/ICollaborationSession.h"
#include <memory>
namespace SagaEditor::Collaboration {
class CollaborationSession final : public ICollaborationSession {
public:
    CollaborationSession();
    ~CollaborationSession() override;
    bool Open(const SessionConfig& cfg) override;
    void Close() override;
    CollaborationSessionState GetState() const noexcept override;
    SessionId GetSessionId()             const noexcept override;
    void SetOnStateChanged(std::function<void(CollaborationSessionState)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
