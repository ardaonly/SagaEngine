/// @file AuthorityManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Authority/AuthorityManager.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct AuthorityManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

AuthorityManager::AuthorityManager()
    : m_impl(std::make_unique<Impl>())
{}

AuthorityManager::~AuthorityManager() = default;

bool AuthorityManager::IsAuthority() const noexcept
{
    return false;
}

uint64_t AuthorityManager::GetAuthorityUserId() const noexcept
{
    return 0;
}

void AuthorityManager::RequestAuthority()
{
    /* stub */
}

void AuthorityManager::ReleaseAuthority()
{
    /* stub */
}

void AuthorityManager::SetOnAuthorityChanged(std::function<void(const AuthorityEvent&)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
