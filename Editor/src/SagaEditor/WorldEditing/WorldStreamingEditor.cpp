/// @file WorldStreamingEditor.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/WorldEditing/WorldStreamingEditor.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct WorldStreamingEditor::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

WorldStreamingEditor::WorldStreamingEditor()
    : m_impl(std::make_unique<Impl>())
{}

WorldStreamingEditor::~WorldStreamingEditor() = default;

void WorldStreamingEditor::SetStreamingRadius(float /*radius*/)
{
    /* stub */
}

float WorldStreamingEditor::GetStreamingRadius() const noexcept
{
    return 0;
}

void WorldStreamingEditor::RefreshStreamingVolumes()
{
    /* stub */
}

void WorldStreamingEditor::SetOnStreamingChanged(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
