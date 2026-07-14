/// @file PrefabEditor.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/PrefabEditing/PrefabEditor.h"

namespace SagaEditor
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PrefabEditor::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

PrefabEditor::PrefabEditor()
    : m_impl(std::make_unique<Impl>())
{}

PrefabEditor::~PrefabEditor() = default;

bool PrefabEditor::OpenPrefab(const std::string& /*prefabPath*/)
{
    return false;
}

void PrefabEditor::ClosePrefab()
{
    /* stub */
}

bool PrefabEditor::IsEditing() const noexcept
{
    return false;
}

void PrefabEditor::ApplyChanges()
{
    /* stub */
}

void PrefabEditor::RevertChanges()
{
    /* stub */
}

} // namespace SagaEditor
