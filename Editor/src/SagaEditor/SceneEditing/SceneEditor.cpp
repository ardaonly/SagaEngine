/// @file SceneEditor.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/SceneEditing/SceneEditor.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct SceneEditor::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

SceneEditor::SceneEditor()
    : m_impl(std::make_unique<Impl>())
{}

SceneEditor::~SceneEditor() = default;

bool SceneEditor::OpenScene(const std::string& /*path*/)
{
    return false;
}

void SceneEditor::CloseScene()
{
    /* stub */
}

bool SceneEditor::SaveScene()
{
    return false;
}

bool SceneEditor::IsModified() const noexcept
{
    return false;
}

uint64_t SceneEditor::CreateEntity(const std::string& /*name*/)
{
    return 0;
}

void SceneEditor::DestroyEntity(uint64_t /*entityId*/)
{
    /* stub */
}

void SceneEditor::SetOnSceneChanged(std::function<void()> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor
