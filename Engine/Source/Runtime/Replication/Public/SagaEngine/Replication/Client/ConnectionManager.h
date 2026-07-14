/// @file ConnectionManager.h
/// @brief Multi-client session table — accept, track, and dispatch all connections.
///
/// Layer  : Server / Networking / Client
/// Purpose: ConnectionManager owns the authoritative session table mapping
///          ClientId → ServerConnection. It mediates between ZoneServer's accept
///          loop and individual connection lifecycles, enforces the client cap,
///          drives session cleanup, and exposes a thread-safe iteration API for
///          the tick loop to flush replication packets.
///
/// Threading:
///   - AcceptSocket() is called from the accept strand (io thread).
///   - DrainInboundPackets() and ForEachConnectedClient() are called from the
///     tick thread.
///   - The session table is protected by a shared mutex: readers (ForEach,
///     DrainInbound) take a shared lock; writers (Accept, Remove) take exclusive.

#pragma once

#include "SagaServer/Networking/Server/ServerConnection.h"
#include "SagaServer/Networking/Core/NetworkTypes.h"

#include <asio.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaServer::Networking
{

// Bring engine networking types into scope
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::ConnectionId;

// ─── ConnectionSendFn ─────────────────────────────────────────────────────────

/// Send functor handed to the tick thread for zero-copy packet dispatch.
using ConnectionSendFn = std::function<void(const uint8_t*, std::size_t)>;

// ─── ConnectionManagerConfig ──────────────────────────────────────────────────

/// Tunable parameters for the connection manager.
struct ConnectionManagerConfig
{
    uint32_t maxClients{1000};              ///< Hard cap; connections beyond this are rejected
    uint32_t heartbeatIntervalMs{1000};     ///< Forwarded to each ServerConnection
    uint32_t connectionTimeoutMs{10000};    ///< Forwarded to each ServerConnection
    uint32_t handshakeTimeoutMs{5000};      ///< Forwarded to each ServerConnection
    uint32_t receiveBufferSize{65536};      ///< Per-connection receive ring size
    uint32_t sendQueueCapacity{524288};     ///< Per-connection max queued bytes (512 KiB)
    uint32_t maxConcurrentSends{16};        ///< Per-connection max outstanding async_writes
    uint32_t inboundPacketQueueDepth{4096}; ///< Per-connection inbound queue depth before drop
};

// ─── ConnectionManagerStats ───────────────────────────────────────────────────

/// Aggregate counters across all sessions.
struct ConnectionManagerStats
{
    std::atomic<uint64_t> totalAccepted{0};
    std::atomic<uint64_t> totalRejectedAtCap{0};
    std::atomic<uint64_t> totalDisconnected{0};
    std::atomic<uint64_t> totalPacketsDispatched{0};
    std::atomic<uint32_t> currentConnected{0};
};

// ─── Callbacks ────────────────────────────────────────────────────────────────

using OnClientConnectedCallback    = std::function<void(ClientId)>;
using OnClientDisconnectedCallback = std::function<void(ClientId)>;
using OnPacketReceivedCallback     = std::function<void(ClientId, const uint8_t*, std::size_t)>;

// ─── InboundPacket ────────────────────────────────────────────────────────────

/// Buffered inbound packet waiting for tick-thread processing.
struct InboundPacket
{
    ClientId             clientId{0};
    std::vector<uint8_t> data;
};

// ─── ConnectionManager ────────────────────────────────────────────────────────

/// Owns the full session table and coordinates all per-client connection objects.
class ConnectionManager final
{
public:
    ConnectionManager(ConnectionManagerConfig config,
                      asio::io_context&       ioContext);

    ~ConnectionManager();

    ConnectionManager(const ConnectionManager&)            = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /// Allocate internal resources. Must be called before AcceptSocket().
    [[nodiscard]] bool Initialize();

    /// Disconnect all sessions and release resources.
    void Shutdown();

    // ── Accept ────────────────────────────────────────────────────────────────

    /// Called from the ZoneServer accept handler — wraps the raw socket in a
    /// ServerConnection and inserts it into the session table.
    void AcceptSocket(asio::ip::tcp::socket socket);

    // ── Tick-thread API ───────────────────────────────────────────────────────

    /// Drain all inbound packets accumulated since the last tick and invoke
    /// the callback for each. Called exclusively from the tick thread.
    void DrainInboundPackets(OnPacketReceivedCallback callback);

    /// Iterate all Connected sessions and invoke the callback.
    /// The callback receives the ClientId and a bound send functor.
    void ForEachConnectedClient(
        std::function<void(ClientId, ConnectionSendFn)> callback) const;

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] bool     HasClient(ClientId clientId)     const;
    [[nodiscard]] uint32_t GetConnectedCount()              const noexcept;
    [[nodiscard]] bool     IsConnected(ClientId clientId)   const;

    /// Retrieve connection stats snapshot for a specific client.
    /// Returns false if the client is not in the session table.
    [[nodiscard]] bool GetClientStats(ClientId clientId,
                                      ServerConnectionStats& out) const;

    [[nodiscard]] const ConnectionManagerStats& GetStats() const noexcept;

    // ── External disconnect ────────────────────────────────────────────────────

    /// Initiate graceful disconnect for a specific client.
    void DisconnectClient(ClientId clientId, int reason = 0);

    /// Disconnect all clients immediately.
    void DisconnectAll(int reason = 0);

    // ── Callbacks ─────────────────────────────────────────────────────────────

    void SetOnClientConnected(OnClientConnectedCallback cb);
    void SetOnClientDisconnected(OnClientDisconnectedCallback cb);
    void SetOnPacketReceived(OnPacketReceivedCallback cb);

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    [[nodiscard]] ClientId AllocateClientId();
    void RemoveClient(ClientId clientId);

    void OnConnectionPacket(ClientId clientId, const uint8_t* data, std::size_t size);
    void OnConnectionDisconnected(ClientId clientId, int reason);

    // ── State ─────────────────────────────────────────────────────────────────

    ConnectionManagerConfig m_config;
    asio::io_context&       m_ioContext;
    ConnectionManagerStats  m_stats;

    mutable std::shared_mutex                                m_sessionMutex;
    std::unordered_map<ClientId,
        std::shared_ptr<ServerConnection>>                   m_sessions;

    // ── Client ID generation ──────────────────────────────────────────────────

    std::atomic<ClientId> m_nextClientId{1};

    // ── Inbound packet queue (tick-thread drain) ───────────────────────────────

    mutable std::mutex         m_inboundMutex;
    std::vector<InboundPacket> m_inboundQueue;     ///< IO threads write here
    std::vector<InboundPacket> m_inboundSwap;      ///< Tick thread drains this

    // ── Callbacks ─────────────────────────────────────────────────────────────

    OnClientConnectedCallback    m_onClientConnected;
    OnClientDisconnectedCallback m_onClientDisconnected;
    OnPacketReceivedCallback     m_onPacketReceived;

    bool m_initialized{false};
    bool m_shutdown{false};
};

} // namespace SagaServer::Networking
