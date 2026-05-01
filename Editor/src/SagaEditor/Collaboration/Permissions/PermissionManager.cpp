/// @file PermissionManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Permissions/PermissionManager.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PermissionManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

PermissionManager::PermissionManager()
    : m_impl(std::make_unique<Impl>())
{}

PermissionManager::~PermissionManager() = default;

CollaboratorRole PermissionManager::GetRole(uint64_t /*userId*/) const noexcept
{
    return {};
}

void PermissionManager::SetRole(uint64_t /*userId*/, CollaboratorRole /*role*/)
{
    /* stub */
}

bool PermissionManager::CanPerform(uint64_t /*userId*/, EditorAction /*action*/) const noexcept
{
    return false;
}

} // namespace SagaEditor::Collaboration
