/// @file CollaborationServer.cpp
/// @brief In-process collaboration server — minimal scaffolding implementation.

#include "SagaEditor/Collaboration/Server/CollaborationServer.h"

namespace SagaEditor::Collaboration
{

// ─── Internal State ───────────────────────────────────────────────────────────

struct CollaborationServer::Impl
{
    CollaborationServerState  state;
    CollaborationServerRouter router;
};

// ─── Construction ─────────────────────────────────────────────────────────────

CollaborationServer::CollaborationServer()
    : m_impl(std::make_unique<Impl>())
{}

CollaborationServer::~CollaborationServer() = default;

CollaborationServer::CollaborationServer(CollaborationServer&&) noexcept            = default;
CollaborationServer& CollaborationServer::operator=(CollaborationServer&&) noexcept = default;

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool CollaborationServer::Start(std::uint16_t port)
{
    if (m_impl->state.isRunning)
    {
        // The transport is single-bind; refuse a second Start so the
        // caller has to Stop first. This keeps state.port honest.
        return false;
    }
    m_impl->state.isRunning   = true;
    m_impl->state.port        = port;
    m_impl->state.clientCount = 0;
    return true;
}

void CollaborationServer::Stop()
{
    m_impl->state.isRunning   = false;
    m_impl->state.clientCount = 0;
    // Port and sessionId are deliberately preserved so a follow-up
    // diagnostics dump can show the last bind.
}

bool CollaborationServer::IsRunning() const noexcept
{
    return m_impl->state.isRunning;
}

// ─── Messaging ────────────────────────────────────────────────────────────────

void CollaborationServer::BroadcastMessage(const std::string& /*msg*/)
{
    // Networking transport is not wired in this scaffolding; the
    // routing-table implementation lives next to this method and
    // will publish messages once the transport lands.
    if (!m_impl->state.isRunning)
    {
        return;
    }
}

std::uint32_t CollaborationServer::GetConnectedClientCount() const noexcept
{
    return m_impl->state.clientCount;
}

// ─── Local Surface ────────────────────────────────────────────────────────────

CollaborationServerRouter& CollaborationServer::Router() noexcept
{
    return m_impl->router;
}

const CollaborationServerRouter& CollaborationServer::Router() const noexcept
{
    return m_impl->router;
}

CollaborationServerState CollaborationServer::GetState() const
{
    return m_impl->state;
}

} // namespace SagaEditor::Collaboration
