/// @file PresenceManager.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Presence/PresenceManager.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PresenceManager::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

PresenceManager::PresenceManager()
    : m_impl(std::make_unique<Impl>())
{}

PresenceManager::~PresenceManager() = default;

void PresenceManager::UpdateLocalPresence(const PresenceInfo& /*info*/)
{
    /* stub */
}

std::vector<PresenceInfo> PresenceManager::GetAllPresence() const
{
    return {};
}

void PresenceManager::SetOnPresenceChanged(std::function<void(const PresenceEvent&)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
