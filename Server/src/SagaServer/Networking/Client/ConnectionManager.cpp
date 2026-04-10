/// @file ConnectionManager.cpp
/// @brief ConnectionManager — multi-client session table implementation.

#include "SagaServer/Networking/Client/ConnectionManager.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cassert>

namespace SagaServer::Networking
{

static constexpr const char* kTag = "ConnectionManager";

// ─── Construction ─────────────────────────────────────────────────────────────

ConnectionManager::ConnectionManager(ConnectionManagerConfig config,
                                     asio::io_context&       ioContext)
    : m_config(config)
    , m_ioContext(ioContext)
{
    m_inboundQueue.reserve(m_config.inboundPacketQueueDepth);
    m_inboundSwap.reserve(m_config.inboundPacketQueueDepth);
}

ConnectionManager::~ConnectionManager()
{
    if (!m_shutdown)
        Shutdown();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ConnectionManager::Initialize()
{
    if (m_initialized)
    {
        LOG_WARN(kTag, "Initialize() called more than once");
        return true;
    }

    m_sessions.reserve(m_config.maxClients);
    m_initialized = true;

    LOG_INFO(kTag, "Initialized — maxClients=%u, heartbeat=%u ms, timeout=%u ms",
             m_config.maxClients,
             m_config.heartbeatIntervalMs,
             m_config.connectionTimeoutMs);
    return true;
}

void ConnectionManager::Shutdown()
{
    if (m_shutdown)
        return;

    m_shutdown = true;

    LOG_INFO(kTag, "Shutting down — disconnecting %zu session(s)",
             [this]()
             {
                 std::shared_lock lock(m_sessionMutex);
                 return m_sessions.size();
             }());

    DisconnectAll(0);

    std::unique_lock lock(m_sessionMutex);
    m_sessions.clear();

    LOG_INFO(kTag, "Shutdown complete");
}

// ─── Accept ───────────────────────────────────────────────────────────────────

void ConnectionManager::AcceptSocket(asio::ip::tcp::socket socket)
{
    if (m_shutdown)
    {
        asio::error_code ec;
        socket.close(ec);
        return;
    }

    const uint32_t current = m_stats.currentConnected.load(std::memory_order_relaxed);
    if (current >= m_config.maxClients)
    {
        m_stats.totalRejectedAtCap.fetch_add(1, std::memory_order_relaxed);
        LOG_WARN(kTag, "Client cap reached (%u/%u) — rejecting connection",
                 current, m_config.maxClients);
        asio::error_code ec;
        socket.close(ec);
        return;
    }

    const ClientId clientId = AllocateClientId();

    ServerConnectionConfig connCfg;
    connCfg.heartbeatIntervalMs = m_config.heartbeatIntervalMs;
    connCfg.connectionTimeoutMs = m_config.connectionTimeoutMs;
    connCfg.handshakeTimeoutMs  = m_config.handshakeTimeoutMs;
    connCfg.maxSendQueueBytes   = m_config.sendQueueCapacity;
    connCfg.receiveBufferBytes  = m_config.receiveBufferSize;
    connCfg.maxConcurrentSends  = m_config.maxConcurrentSends;

    auto conn = std::make_shared<ServerConnection>(
        clientId, std::move(socket), m_ioContext, connCfg);

    conn->SetOnPacketReceived(
        [this](ClientId cid, const uint8_t* data, std::size_t size)
        {
            OnConnectionPacket(cid, data, size);
        });

    conn->SetOnDisconnected(
        [this](ClientId cid, int reason)
        {
            OnConnectionDisconnected(cid, reason);
        });

    {
        std::unique_lock lock(m_sessionMutex);
        m_sessions[clientId] = conn;
    }

    m_stats.totalAccepted.fetch_add(1, std::memory_order_relaxed);
    m_stats.currentConnected.fetch_add(1, std::memory_order_relaxed);

    conn->Start();

    if (m_onClientConnected)
        m_onClientConnected(clientId);

    LOG_DEBUG(kTag, "Client %llu accepted — %u/%u connected",
              static_cast<unsigned long long>(clientId),
              m_stats.currentConnected.load(),
              m_config.maxClients);
}

// ─── Tick-thread API ──────────────────────────────────────────────────────────

void ConnectionManager::DrainInboundPackets(OnPacketReceivedCallback callback)
{
    {
        std::lock_guard<std::mutex> lock(m_inboundMutex);
        if (m_inboundQueue.empty())
            return;
        std::swap(m_inboundQueue, m_inboundSwap);
    }

    for (const auto& pkt : m_inboundSwap)
    {
        if (callback)
            callback(pkt.clientId, pkt.data.data(), pkt.data.size());
        m_stats.totalPacketsDispatched.fetch_add(1, std::memory_order_relaxed);
    }

    m_inboundSwap.clear();
}

void ConnectionManager::ForEachConnectedClient(
    std::function<void(ClientId, ConnectionSendFn)> callback) const
{
    std::shared_lock lock(m_sessionMutex);

    for (const auto& [clientId, conn] : m_sessions)
    {
        if (!conn || !conn->IsConnected())
            continue;

        // Capture by shared_ptr so the connection outlives the lambda.
        ConnectionSendFn sendFn = [connWeak = std::weak_ptr<ServerConnection>(conn)]
            (const uint8_t* data, std::size_t size)
        {
            if (auto c = connWeak.lock())
                c->Send(data, size);
        };

        callback(clientId, std::move(sendFn));
    }
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool ConnectionManager::HasClient(ClientId clientId) const
{
    std::shared_lock lock(m_sessionMutex);
    return m_sessions.count(clientId) > 0;
}

uint32_t ConnectionManager::GetConnectedCount() const noexcept
{
    return m_stats.currentConnected.load(std::memory_order_relaxed);
}

bool ConnectionManager::IsConnected(ClientId clientId) const
{
    std::shared_lock lock(m_sessionMutex);
    const auto it = m_sessions.find(clientId);
    if (it == m_sessions.end() || !it->second)
        return false;
    return it->second->IsConnected();
}

bool ConnectionManager::GetClientStats(ClientId              clientId,
                                        ServerConnectionStats& out) const
{
    std::shared_lock lock(m_sessionMutex);
    const auto it = m_sessions.find(clientId);
    if (it == m_sessions.end() || !it->second)
        return false;

    const auto& src = it->second->GetStats();
    out.bytesSent.store(src.bytesSent.load());
    out.bytesReceived.store(src.bytesReceived.load());
    out.packetsSent.store(src.packetsSent.load());
    out.packetsReceived.store(src.packetsReceived.load());
    out.sendQueueDrops.store(src.sendQueueDrops.load());
    out.heartbeatsSent.store(src.heartbeatsSent.load());
    out.rttMs.store(src.rttMs.load());
    return true;
}

const ConnectionManagerStats& ConnectionManager::GetStats() const noexcept
{
    return m_stats;
}

// ─── External disconnect ──────────────────────────────────────────────────────

void ConnectionManager::DisconnectClient(ClientId clientId, int reason)
{
    std::shared_ptr<ServerConnection> conn;
    {
        std::shared_lock lock(m_sessionMutex);
        const auto it = m_sessions.find(clientId);
        if (it != m_sessions.end())
            conn = it->second;
    }

    if (conn)
        conn->Shutdown(reason);
}

void ConnectionManager::DisconnectAll(int reason)
{
    std::vector<std::shared_ptr<ServerConnection>> snapshot;
    {
        std::shared_lock lock(m_sessionMutex);
        snapshot.reserve(m_sessions.size());
        for (const auto& [id, conn] : m_sessions)
            if (conn) snapshot.push_back(conn);
    }

    for (auto& conn : snapshot)
        conn->Shutdown(reason);
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void ConnectionManager::SetOnClientConnected(OnClientConnectedCallback cb)
{
    m_onClientConnected = std::move(cb);
}

void ConnectionManager::SetOnClientDisconnected(OnClientDisconnectedCallback cb)
{
    m_onClientDisconnected = std::move(cb);
}

void ConnectionManager::SetOnPacketReceived(OnPacketReceivedCallback cb)
{
    m_onPacketReceived = std::move(cb);
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

ClientId ConnectionManager::AllocateClientId()
{
    return m_nextClientId.fetch_add(1, std::memory_order_relaxed);
}

void ConnectionManager::RemoveClient(ClientId clientId)
{
    std::unique_lock lock(m_sessionMutex);
    const std::size_t erased = m_sessions.erase(clientId);

    if (erased > 0)
    {
        const uint32_t prev = m_stats.currentConnected.fetch_sub(1, std::memory_order_relaxed);
        if (prev == 0)
            m_stats.currentConnected.store(0, std::memory_order_relaxed);

        m_stats.totalDisconnected.fetch_add(1, std::memory_order_relaxed);
    }
}

void ConnectionManager::OnConnectionPacket(ClientId       clientId,
                                            const uint8_t* data,
                                            std::size_t    size)
{
    if (size == 0 || m_shutdown)
        return;

    InboundPacket pkt;
    pkt.clientId = clientId;
    pkt.data.assign(data, data + size);

    std::lock_guard<std::mutex> lock(m_inboundMutex);

    if (m_inboundQueue.size() >= m_config.inboundPacketQueueDepth)
    {
        LOG_WARN(kTag, "Inbound queue full — dropping packet from client %llu",
                 static_cast<unsigned long long>(clientId));
        return;
    }

    m_inboundQueue.push_back(std::move(pkt));
}

void ConnectionManager::OnConnectionDisconnected(ClientId clientId, int reason)
{
    LOG_DEBUG(kTag, "Client %llu disconnected (reason=%d)", 
              static_cast<unsigned long long>(clientId), reason);

    RemoveClient(clientId);

    if (m_onClientDisconnected)
        m_onClientDisconnected(clientId);
}

} // namespace SagaServer::Networking
