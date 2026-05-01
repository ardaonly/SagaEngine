/// @file CrdtSceneDocument.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Sync/CrdtSceneDocument.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct CrdtSceneDocument::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

CrdtSceneDocument::CrdtSceneDocument()
    : m_impl(std::make_unique<Impl>())
{}

CrdtSceneDocument::~CrdtSceneDocument() = default;

void CrdtSceneDocument::Apply(const EditOperation& /*op*/)
{
    /* stub */
}

std::string CrdtSceneDocument::Serialize() const
{
    return {};
}

bool CrdtSceneDocument::Deserialize(const std::string& /*data*/)
{
    return false;
}

} // namespace SagaEditor::Collaboration
