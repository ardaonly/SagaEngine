/// @file PeerRegistry.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Client/PeerRegistry.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct PeerRegistry::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

PeerRegistry::PeerRegistry()
    : m_impl(std::make_unique<Impl>())
{}

PeerRegistry::~PeerRegistry() = default;

void PeerRegistry::AddPeer(const PeerInfo& /*peer*/)
{
    /* stub */
}

void PeerRegistry::RemovePeer(uint64_t /*userId*/)
{
    /* stub */
}

const PeerInfo* PeerRegistry::FindPeer(uint64_t /*userId*/) const noexcept
{
    return nullptr;
}

std::vector<PeerInfo> PeerRegistry::GetOnlinePeers() const
{
    return {};
}

} // namespace SagaEditor::Collaboration
