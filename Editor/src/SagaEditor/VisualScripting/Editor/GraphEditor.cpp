/// @file GraphEditor.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/VisualScripting/Editor/GraphEditor.h"

namespace SagaEditor::VisualScripting
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct GraphEditor::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

GraphEditor::GraphEditor(GraphDocument& /*doc*/)
    : m_impl(std::make_unique<Impl>())
{}

GraphEditor::~GraphEditor() = default;

void GraphEditor::Open()
{
    /* stub */
}

void GraphEditor::Close()
{
    /* stub */
}

bool GraphEditor::IsOpen() const noexcept
{
    return false;
}

} // namespace SagaEditor::VisualScripting
