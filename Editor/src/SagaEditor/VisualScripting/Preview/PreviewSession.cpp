/// @file PreviewSession.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Preview/PreviewSession.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PreviewSession::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

PreviewSession::PreviewSession(GraphDocument& /*doc*/)
    : m_impl(std::make_unique<Impl>())
{}

PreviewSession::~PreviewSession() = default;

bool PreviewSession::Start()
{
    return false;
}

void PreviewSession::Stop()
{
    /* stub */
}

bool PreviewSession::IsRunning() const noexcept
{
    return false;
}

void PreviewSession::Tick(float /*dt*/)
{
    /* stub */
}

} // namespace SagaEditor::VisualScripting
