/// @file SyncTransportImpl.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Sync/SyncTransportImpl.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct SyncTransportImpl::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

SyncTransportImpl::SyncTransportImpl()
    : m_impl(std::make_unique<Impl>())
{}

SyncTransportImpl::~SyncTransportImpl() = default;

void SyncTransportImpl::Send(const EditOperation& /*op*/)
{
    /* stub */
}

void SyncTransportImpl::SetOnReceive(std::function<void(const EditOperation&)> /*cb*/)
{
    /* stub */
}

bool SyncTransportImpl::IsConnected() const noexcept
{
    return false;
}

} // namespace SagaEditor::Collaboration
