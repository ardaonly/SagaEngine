/// @file LockManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Locks/LockManager.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct LockManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

LockManager::LockManager()
    : m_impl(std::make_unique<Impl>())
{}

LockManager::~LockManager() = default;

bool LockManager::TryAcquire(const std::string& /*objectId*/, LockPolicy /*policy*/, LockToken& /*out*/)
{
    return false;
}

void LockManager::Release(uint64_t /*lockId*/)
{
    /* stub */
}

bool LockManager::IsLocked(const std::string& /*objectId*/) const noexcept
{
    return false;
}

void LockManager::SetOnLockEvent(std::function<void(const LockEvent&)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
