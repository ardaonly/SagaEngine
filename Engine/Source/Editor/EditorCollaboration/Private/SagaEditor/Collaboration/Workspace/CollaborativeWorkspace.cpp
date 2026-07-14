/// @file CollaborativeWorkspace.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Workspace/CollaborativeWorkspace.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct CollaborativeWorkspace::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

CollaborativeWorkspace::CollaborativeWorkspace()
    : m_impl(std::make_unique<Impl>())
{}

CollaborativeWorkspace::~CollaborativeWorkspace() = default;

std::vector<WorkspaceDocument> CollaborativeWorkspace::GetDocuments() const
{
    return {};
}

WorkspaceDocument* CollaborativeWorkspace::FindDocument(const std::string& /*id*/)
{
    return nullptr;
}

void CollaborativeWorkspace::MarkModified(const std::string& /*documentId*/)
{
    /* stub */
}

void CollaborativeWorkspace::Sync()
{
    /* stub */
}

void CollaborativeWorkspace::SetOnDocumentChanged(std::function<void(const WorkspaceDocument&)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
