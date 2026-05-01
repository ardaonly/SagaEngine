/// @file CollaborationClient.cpp
/// @brief Pimpl scaffolding — auto-generated stub bodies.

#include "SagaEditor/Collaboration/Client/CollaborationClient.h"

namespace SagaEditor::Collaboration
{

// ─── Pimpl Definition ────────────────────────────────────────────────────────

struct CollaborationClient::Impl {};

// ─── Method Stubs ─────────────────────────────────────────────────────────────

CollaborationClient::CollaborationClient()
    : m_impl(std::make_unique<Impl>())
{}

CollaborationClient::~CollaborationClient() = default;

bool CollaborationClient::Connect(const std::string& /*address*/, uint16_t /*port*/)
{
    return false;
}

void CollaborationClient::Disconnect()
{
    /* stub */
}

bool CollaborationClient::IsConnected() const noexcept
{
    return false;
}

void CollaborationClient::SendMessage(const std::string& /*msg*/)
{
    /* stub */
}

void CollaborationClient::SetOnMessage(std::function<void(const std::string&)> /*cb*/)
{
    /* stub */
}

void CollaborationClient::SetOnPeerJoined(std::function<void(const PeerInfo&)> /*cb*/)
{
    /* stub */
}

void CollaborationClient::SetOnPeerLeft(std::function<void(uint64_t)> /*cb*/)
{
    /* stub */
}

} // namespace SagaEditor::Collaboration
