/// @file GraphEvaluationRunner.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualBlocks/Evaluation/GraphEvaluationRunner.h"

namespace SagaEditor::VisualBlocks
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct GraphEvaluationRunner::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

GraphEvaluationRunner::GraphEvaluationRunner(GraphDocument& /*doc*/)
    : m_impl(std::make_unique<Impl>())
{}

GraphEvaluationRunner::~GraphEvaluationRunner() = default;

void GraphEvaluationRunner::Run()
{
    /* stub */
}

void GraphEvaluationRunner::Stop()
{
    /* stub */
}

bool GraphEvaluationRunner::IsRunning() const noexcept
{
    return false;
}

void GraphEvaluationRunner::SetOnStep(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::VisualBlocks
