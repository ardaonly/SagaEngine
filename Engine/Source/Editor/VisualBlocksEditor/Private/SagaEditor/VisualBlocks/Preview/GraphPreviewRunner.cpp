/// @file GraphPreviewRunner.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Preview/GraphPreviewRunner.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct GraphPreviewRunner::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

GraphPreviewRunner::GraphPreviewRunner(GraphDocument& /*doc*/)
    : m_impl(std::make_unique<Impl>())
{}

GraphPreviewRunner::~GraphPreviewRunner() = default;

void GraphPreviewRunner::Run()
{
    /* stub */
}

void GraphPreviewRunner::Stop()
{
    /* stub */
}

bool GraphPreviewRunner::IsRunning() const noexcept
{
    return false;
}

void GraphPreviewRunner::SetOnStep(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::VisualScripting
