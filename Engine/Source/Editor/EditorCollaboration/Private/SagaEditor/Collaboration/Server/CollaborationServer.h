/// @file CollaborationServer.h
/// @brief Concrete in-process implementation of `ICollaborationServer`.

#pragma once

#include "SagaEditor/Collaboration/Server/CollaborationServerRouter.h"
#include "SagaEditor/Collaboration/Server/CollaborationServerState.h"
#include "SagaEditor/Collaboration/Server/ICollaborationServer.h"

#include <cstdint>
#include <memory>
#include <string>

namespace SagaEditor::Collaboration
{

// ─── Collaboration Server ─────────────────────────────────────────────────────

/// In-process collaboration server used by the editor's authoring
/// session. The class exposes the framework-free contract declared
/// by `ICollaborationServer` and forwards inbound traffic into
/// `CollaborationServerRouter`. Networking transport details (TCP,
/// QUIC, named pipes) live behind a pimpl so this header stays free
/// of any backend dependency.
class CollaborationServer : public ICollaborationServer
{
public:
    CollaborationServer();
    ~CollaborationServer() override;

    CollaborationServer(const CollaborationServer&)            = delete;
    CollaborationServer& operator=(const CollaborationServer&) = delete;
    CollaborationServer(CollaborationServer&&) noexcept;
    CollaborationServer& operator=(CollaborationServer&&) noexcept;

    // ─── ICollaborationServer ─────────────────────────────────────────────────

    /// Bind the server to `port` and start accepting clients. Returns
    /// false when the server is already running or when the underlying
    /// transport refuses the bind. Calling `Start` twice without an
    /// intervening `Stop` is a no-op that returns false.
    [[nodiscard]] bool Start(std::uint16_t port) override;

    /// Stop accepting new clients and tear down existing sessions.
    /// Safe to call when the server is not running.
    void Stop() override;

    /// True when the server is currently bound and accepting clients.
    [[nodiscard]] bool IsRunning() const noexcept override;

    /// Send `msg` to every connected client. Silent no-op when the
    /// server is not running so callers do not have to gate the call
    /// at every site.
    void BroadcastMessage(const std::string& msg) override;

    /// Number of currently connected clients.
    [[nodiscard]] std::uint32_t GetConnectedClientCount() const noexcept override;

    // ─── Local Surface ────────────────────────────────────────────────────────

    /// Routing table that decides which handler runs for a given
    /// inbound message type. Reserved for future hand-off to the
    /// collaboration host; today the router is wired but inactive.
    [[nodiscard]] CollaborationServerRouter&       Router()       noexcept;
    [[nodiscard]] const CollaborationServerRouter& Router() const noexcept;

    /// Snapshot of the current server state (running flag, port,
    /// client count, session id). Returned by value so callers
    /// cannot hold a stale reference across a `Stop` / `Start` cycle.
    [[nodiscard]] CollaborationServerState GetState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace SagaEditor::Collaboration
