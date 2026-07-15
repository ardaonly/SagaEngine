/// @file EvaluationSession.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualBlocks/Evaluation/EvaluationSession.h"

namespace SagaEditor::VisualBlocks
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct EvaluationSession::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

EvaluationSession::EvaluationSession(GraphDocument& /*doc*/)
    : m_impl(std::make_unique<Impl>())
{}

EvaluationSession::~EvaluationSession() = default;

bool EvaluationSession::Start()
{
    return false;
}

void EvaluationSession::Stop()
{
    /* stub */
}

bool EvaluationSession::IsRunning() const noexcept
{
    return false;
}

void EvaluationSession::Tick(float /*dt*/)
{
    /* stub */
}

} // namespace SagaEditor::VisualBlocks
