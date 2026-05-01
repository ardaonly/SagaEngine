/// @file CollaborationSession.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Session/CollaborationSession.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct CollaborationSession::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

CollaborationSession::CollaborationSession()
    : m_impl(std::make_unique<Impl>())
{}

CollaborationSession::~CollaborationSession() = default;

bool CollaborationSession::Open(const SessionConfig& /*cfg*/)
{
    return false;
}

void CollaborationSession::Close()
{
    /* stub */
}

CollaborationSessionState CollaborationSession::GetState() const noexcept
{
    return {};
}

SessionId CollaborationSession::GetSessionId() const noexcept
{
    return {};
}

void CollaborationSession::SetOnStateChanged(std::function<void(CollaborationSessionState)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
