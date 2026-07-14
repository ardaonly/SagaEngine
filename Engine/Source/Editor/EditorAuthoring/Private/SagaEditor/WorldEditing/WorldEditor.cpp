/// @file WorldEditor.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/WorldEditing/WorldEditor.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct WorldEditor::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

WorldEditor::WorldEditor()
    : m_impl(std::make_unique<Impl>())
{}

WorldEditor::~WorldEditor() = default;

bool WorldEditor::OpenWorld(const std::string& /*path*/)
{
    return false;
}

void WorldEditor::CloseWorld()
{
    /* stub */
}

bool WorldEditor::SaveWorld()
{
    return false;
}

bool WorldEditor::IsModified() const noexcept
{
    return false;
}

void WorldEditor::SetOnWorldChanged(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
