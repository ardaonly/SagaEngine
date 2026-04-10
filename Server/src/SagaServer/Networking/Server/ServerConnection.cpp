/// @file ServerConnection.cpp
/// @brief ServerConnection — per-client TCP connection with full state machine.

#include "SagaServer/Networking/Server/ServerConnection.h"
#include "SagaEngine/Core/Log/Log.h"

#include <cassert>
#include <chrono>
#include <cstring>

namespace SagaServer::Networking
{

using namespace std::chrono_literals;

static constexpr const char* kTag = "ServerConnection";

// Wire layout of our framing header: [magic:2][type:1][flags:1][bodyLen:4]
static constexpr uint16_t kPacketMagic   = 0x5347; // 'SG'
static constexpr uint32_t kMaxBodyBytes  = 65000;   // Reject anything larger

// ─── Construction ─────────────────────────────────────────────────────────────

ServerConnection::ServerConnection(ClientId               clientId,
                                   asio::ip::tcp::socket  socket,
                                   asio::io_context&      ioContext,
                                   ServerConnectionConfig config)
    : m_clientId(clientId)
    , m_socket(std::move(socket))
    , m_strand(asio::make_strand(ioContext))
    , m_config(config)
    , m_heartbeatTimer(ioContext)
    , m_timeoutTimer(ioContext)
    , m_handshakeTimer(ioContext)
    , m_connectTime(std::chrono::steady_clock::now())
{
    m_lastReceivedTime.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed);

    m_headerBuf.resize(kHeaderSize);
    m_bodyBuf.reserve(4096);
}

ServerConnection::~ServerConnection()
{
    asio::error_code ec;
    m_heartbeatTimer.cancel(ec);
    m_timeoutTimer.cancel(ec);
    m_handshakeTimer.cancel(ec);
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void ServerConnection::Start()
{
    TransitionTo(ServerConnectionState::Handshaking);
    StartHandshakeTimer();
    SendHandshakeChallenge();
    StartReceiveHeader();
}

void ServerConnection::Shutdown(int reason)
{
    asio::post(m_strand, [self = shared_from_this(), reason]()
    {
        if (self->m_state.load(std::memory_order_relaxed) == ServerConnectionState::Disconnected)
            return;

        self->TransitionTo(ServerConnectionState::Draining);

        // Give the send queue a chance to flush before closing.
        // In production, a drain timer can cap the wait to ~500 ms.
        self->HandleDisconnect(reason);
    });
}

void ServerConnection::ForceClose(int reason)
{
    asio::post(m_strand, [self = shared_from_this(), reason]()
    {
        asio::error_code ec;
        self->m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        self->m_socket.close(ec);
        self->HandleDisconnect(reason);
    });
}

// ─── Send ─────────────────────────────────────────────────────────────────────

bool ServerConnection::Send(const uint8_t* data, std::size_t size)
{
    if (m_state.load(std::memory_order_relaxed) == ServerConnectionState::Disconnected)
        return false;

    if (size == 0 || size > kMaxBodyBytes)
    {
        LOG_WARN(kTag, "Client %llu: rejecting send of %zu bytes (invalid size)",
                 static_cast<unsigned long long>(m_clientId), size);
        return false;
    }

    std::vector<uint8_t> buf(size);
    std::memcpy(buf.data(), data, size);

    EnqueueSend(std::move(buf));
    return true;
}

bool ServerConnection::Send(const Packet& packet)
{
    const auto serialized = packet.GetSerializedData();
    if (serialized.empty())
        return false;

    EnqueueSend(std::vector<uint8_t>(serialized.begin(), serialized.end()));
    return true;
}

void ServerConnection::EnqueueSend(std::vector<uint8_t> data)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);

    if (m_sendQueueBytes + data.size() > m_config.maxSendQueueBytes)
    {
        m_stats.sendQueueDrops.fetch_add(1, std::memory_order_relaxed);
        LOG_WARN(kTag, "Client %llu: send queue full (%zu bytes), dropping %zu byte packet",
                 static_cast<unsigned long long>(m_clientId),
                 m_sendQueueBytes,
                 data.size());
        return;
    }

    m_sendQueueBytes += data.size();
    m_sendQueue.push_back(std::move(data));

    if (!m_sendActive)
        TryFlushSendQueue();
}

void ServerConnection::TryFlushSendQueue()
{
    // Caller holds m_sendMutex.
    if (m_sendQueue.empty() || m_sendActive)
        return;

    m_sendActive = true;
    auto& front = m_sendQueue.front();

    auto self = shared_from_this();
    asio::async_write(m_socket,
        asio::buffer(front.data(), front.size()),
        asio::bind_executor(m_strand,
            [self](const asio::error_code& ec, std::size_t bytes)
            {
                self->HandleSendComplete(ec, bytes);
            }));
}

void ServerConnection::HandleSendComplete(const asio::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        LOG_WARN(kTag, "Client %llu: send error: %s",
                 static_cast<unsigned long long>(m_clientId), ec.message().c_str());
        HandleDisconnect(-1);
        return;
    }

    m_stats.bytesSent.fetch_add(bytes, std::memory_order_relaxed);
    m_stats.packetsSent.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(m_sendMutex);
    if (!m_sendQueue.empty())
    {
        m_sendQueueBytes -= m_sendQueue.front().size();
        m_sendQueue.pop_front();
    }

    m_sendActive = false;

    if (!m_sendQueue.empty() &&
        m_state.load(std::memory_order_relaxed) != ServerConnectionState::Disconnected)
    {
        TryFlushSendQueue();
    }
}

// ─── Receive pipeline ─────────────────────────────────────────────────────────

void ServerConnection::StartReceiveHeader()
{
    if (m_state.load(std::memory_order_relaxed) == ServerConnectionState::Disconnected)
        return;

    asio::async_read(m_socket,
        asio::buffer(m_headerBuf.data(), kHeaderSize),
        asio::bind_executor(m_strand,
            [self = shared_from_this()](const asio::error_code& ec, std::size_t bytes)
            {
                self->HandleReceiveHeader(ec, bytes);
            }));
}

void ServerConnection::HandleReceiveHeader(const asio::error_code& ec, std::size_t bytes)
{
    if (ec)
    {
        if (ec != asio::error::operation_aborted && ec != asio::error::eof)
        {
            LOG_WARN(kTag, "Client %llu: receive header error: %s",
                     static_cast<unsigned long long>(m_clientId), ec.message().c_str());
        }
        HandleDisconnect(-1);
        return;
    }

    if (bytes < kHeaderSize)
    {
        LOG_WARN(kTag, "Client %llu: short header (%zu bytes)", 
                 static_cast<unsigned long long>(m_clientId), bytes);
        HandleDisconnect(-2);
        return;
    }

    // Parse framing header: magic[2] type[1] flags[1] bodyLen[4]
    uint16_t magic;
    std::memcpy(&magic, m_headerBuf.data(), sizeof(magic));

    if (magic != kPacketMagic)
    {
        LOG_WARN(kTag, "Client %llu: bad magic 0x%04X — possible protocol mismatch",
                 static_cast<unsigned long long>(m_clientId), magic);
        HandleDisconnect(-3);
        return;
    }

    uint32_t bodyLen;
    std::memcpy(&bodyLen, m_headerBuf.data() + 4, sizeof(bodyLen));

    if (bodyLen > kMaxBodyBytes)
    {
        LOG_WARN(kTag, "Client %llu: body length %u exceeds max %u",
                 static_cast<unsigned long long>(m_clientId), bodyLen, kMaxBodyBytes);
        HandleDisconnect(-4);
        return;
    }

    m_lastReceivedTime.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed);

    if (bodyLen == 0)
    {
        DispatchPacket(m_headerBuf.data(), kHeaderSize);
        StartReceiveHeader();
        return;
    }

    StartReceiveBody(bodyLen);
}

void ServerConnection::StartReceiveBody(uint32_t bodyLength)
{
    m_bodyBuf.resize(kHeaderSize + bodyLength);
    std::memcpy(m_bodyBuf.data(), m_headerBuf.data(), kHeaderSize);

    asio::async_read(m_socket,
        asio::buffer(m_bodyBuf.data() + kHeaderSize, bodyLength),
        asio::bind_executor(m_strand,
            [self = shared_from_this(), bodyLength]
            (const asio::error_code& ec, std::size_t bytes)
            {
                self->HandleReceiveBody(ec, bytes, bodyLength);
            }));
}

void ServerConnection::HandleReceiveBody(const asio::error_code& ec,
                                          std::size_t             bytes,
                                          uint32_t                bodyLength)
{
    if (ec)
    {
        if (ec != asio::error::operation_aborted && ec != asio::error::eof)
        {
            LOG_WARN(kTag, "Client %llu: receive body error: %s",
                     static_cast<unsigned long long>(m_clientId), ec.message().c_str());
        }
        HandleDisconnect(-1);
        return;
    }

    if (bytes < bodyLength)
    {
        LOG_WARN(kTag, "Client %llu: short body (expected %u, got %zu)",
                 static_cast<unsigned long long>(m_clientId), bodyLength, bytes);
        HandleDisconnect(-2);
        return;
    }

    m_stats.bytesReceived.fetch_add(kHeaderSize + bytes, std::memory_order_relaxed);
    m_stats.packetsReceived.fetch_add(1, std::memory_order_relaxed);

    m_lastReceivedTime.store(
        std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed);

    DispatchPacket(m_bodyBuf.data(), kHeaderSize + bodyLength);
    StartReceiveHeader();
}

void ServerConnection::DispatchPacket(const uint8_t* data, std::size_t size)
{
    const uint8_t packetType = (size >= 3) ? data[2] : 0xFF;

    if (m_state.load(std::memory_order_relaxed) == ServerConnectionState::Handshaking)
    {
        ProcessHandshakeResponse(data, size);
        return;
    }

    // Heartbeat ACK: client responded to our ping.
    if (packetType == static_cast<uint8_t>(PacketType::KeepAlive))
    {
        if (size >= kHeaderSize + sizeof(uint64_t))
        {
            uint64_t pingTs;
            std::memcpy(&pingTs, data + kHeaderSize, sizeof(pingTs));
            const auto now = std::chrono::steady_clock::now();
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count());
            const uint32_t rtt = static_cast<uint32_t>(nowMs > pingTs ? nowMs - pingTs : 0);
            m_stats.rttMs.store(rtt, std::memory_order_relaxed);
        }
        return;
    }

    if (m_onPacketReceived)
        m_onPacketReceived(m_clientId, data, size);
}

// ─── Handshake ────────────────────────────────────────────────────────────────

void ServerConnection::SendHandshakeChallenge()
{
    // A production handshake would send a cryptographic challenge here
    // (nonce, server public key, protocol version). For now we send a
    // version packet and wait for the client to echo it back.
    Packet challenge(PacketType::HandshakeRequest);
    challenge.Write(static_cast<uint32_t>(0x0001)); // protocol version
    challenge.Write(m_clientId);

    const auto data = challenge.GetSerializedData();
    EnqueueSend(std::vector<uint8_t>(data.begin(), data.end()));
    LOG_DEBUG(kTag, "Client %llu: handshake challenge sent", 
              static_cast<unsigned long long>(m_clientId));
}

void ServerConnection::ProcessHandshakeResponse(const uint8_t* data, std::size_t size)
{
    if (size < kHeaderSize + sizeof(uint32_t))
    {
        LOG_WARN(kTag, "Client %llu: malformed handshake response (%zu bytes)",
                 static_cast<unsigned long long>(m_clientId), size);
        HandleDisconnect(-10);
        return;
    }

    uint32_t version;
    std::memcpy(&version, data + kHeaderSize, sizeof(version));

    if (version != 0x0001)
    {
        LOG_WARN(kTag, "Client %llu: protocol version mismatch (got 0x%04X, expected 0x0001)",
                 static_cast<unsigned long long>(m_clientId), version);
        HandleDisconnect(-11);
        return;
    }

    asio::error_code ec;
    m_handshakeTimer.cancel(ec);

    TransitionTo(ServerConnectionState::Connected);
    StartHeartbeatTimer();
    StartTimeoutTimer();

    LOG_INFO(kTag, "Client %llu: handshake complete — connection established",
             static_cast<unsigned long long>(m_clientId));
}

// ─── Heartbeat ────────────────────────────────────────────────────────────────

void ServerConnection::StartHeartbeatTimer()
{
    m_heartbeatTimer.expires_after(
        std::chrono::milliseconds(m_config.heartbeatIntervalMs));

    m_heartbeatTimer.async_wait(
        asio::bind_executor(m_strand,
            [self = shared_from_this()](const asio::error_code& ec)
            {
                self->HandleHeartbeatTimer(ec);
            }));
}

void ServerConnection::HandleHeartbeatTimer(const asio::error_code& ec)
{
    if (ec || m_state.load(std::memory_order_relaxed) != ServerConnectionState::Connected)
        return;

    SendHeartbeatPing();
    StartHeartbeatTimer();
}

void ServerConnection::SendHeartbeatPing()
{
    const auto now = std::chrono::steady_clock::now();
    const uint64_t tsMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());

    m_lastPingTimestamp = tsMs;

    Packet ping(PacketType::KeepAlive);
    ping.Write(tsMs);

    const auto data = ping.GetSerializedData();
    EnqueueSend(std::vector<uint8_t>(data.begin(), data.end()));
    m_stats.heartbeatsSent.fetch_add(1, std::memory_order_relaxed);
}

// ─── Timeout ──────────────────────────────────────────────────────────────────

void ServerConnection::StartTimeoutTimer()
{
    m_timeoutTimer.expires_after(
        std::chrono::milliseconds(m_config.connectionTimeoutMs));

    m_timeoutTimer.async_wait(
        asio::bind_executor(m_strand,
            [self = shared_from_this()](const asio::error_code& ec)
            {
                self->HandleTimeoutTimer(ec);
            }));
}

void ServerConnection::HandleTimeoutTimer(const asio::error_code& ec)
{
    if (ec)
        return;

    if (!IsHealthy())
    {
        LOG_WARN(kTag, "Client %llu: connection timed out — no data in %u ms",
                 static_cast<unsigned long long>(m_clientId),
                 m_config.connectionTimeoutMs);
        HandleDisconnect(-20);
        return;
    }

    StartTimeoutTimer();
}

void ServerConnection::StartHandshakeTimer()
{
    m_handshakeTimer.expires_after(
        std::chrono::milliseconds(m_config.handshakeTimeoutMs));

    m_handshakeTimer.async_wait(
        asio::bind_executor(m_strand,
            [self = shared_from_this()](const asio::error_code& ec)
            {
                self->HandleHandshakeTimer(ec);
            }));
}

void ServerConnection::HandleHandshakeTimer(const asio::error_code& ec)
{
    if (ec)
        return;

    if (m_state.load(std::memory_order_relaxed) == ServerConnectionState::Handshaking)
    {
        LOG_WARN(kTag, "Client %llu: handshake timeout after %u ms",
                 static_cast<unsigned long long>(m_clientId),
                 m_config.handshakeTimeoutMs);
        HandleDisconnect(-21);
    }
}

// ─── State transitions ────────────────────────────────────────────────────────

void ServerConnection::TransitionTo(ServerConnectionState newState)
{
    const auto oldState = m_state.exchange(newState, std::memory_order_acq_rel);
    if (oldState == newState)
        return;

    LOG_DEBUG(kTag, "Client %llu: state %u → %u",
              static_cast<unsigned long long>(m_clientId),
              static_cast<uint8_t>(oldState),
              static_cast<uint8_t>(newState));

    if (m_onStateChanged)
        m_onStateChanged(m_clientId, newState);
}

void ServerConnection::HandleDisconnect(int reason)
{
    if (m_disconnectNotified)
        return;

    m_disconnectNotified = true;

    asio::error_code ec;
    m_heartbeatTimer.cancel(ec);
    m_timeoutTimer.cancel(ec);
    m_handshakeTimer.cancel(ec);

    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);

    TransitionTo(ServerConnectionState::Disconnected);

    if (m_onDisconnected)
        m_onDisconnected(m_clientId, reason);

    LOG_DEBUG(kTag, "Client %llu: disconnected (reason=%d)", 
              static_cast<unsigned long long>(m_clientId), reason);
}

// ─── Utilities ────────────────────────────────────────────────────────────────

bool ServerConnection::IsHealthy() const noexcept
{
    const auto lastRx = std::chrono::steady_clock::time_point(
        std::chrono::steady_clock::duration(
            m_lastReceivedTime.load(std::memory_order_relaxed)));

    const auto elapsed = std::chrono::steady_clock::now() - lastRx;
    return elapsed < std::chrono::milliseconds(m_config.connectionTimeoutMs);
}

// ─── Callbacks ────────────────────────────────────────────────────────────────

void ServerConnection::SetOnPacketReceived(ConnectionPacketCallback cb)
{
    m_onPacketReceived = std::move(cb);
}

void ServerConnection::SetOnStateChanged(ConnectionStateCallback cb)
{
    m_onStateChanged = std::move(cb);
}

void ServerConnection::SetOnDisconnected(ConnectionDisconnectCallback cb)
{
    m_onDisconnected = std::move(cb);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

ClientId ServerConnection::GetClientId() const noexcept
{
    return m_clientId;
}

ServerConnectionState ServerConnection::GetState() const noexcept
{
    return m_state.load(std::memory_order_relaxed);
}

bool ServerConnection::IsConnected() const noexcept
{
    return m_state.load(std::memory_order_relaxed) == ServerConnectionState::Connected;
}

std::string ServerConnection::GetRemoteAddress() const
{
    asio::error_code ec;
    const auto ep = m_socket.remote_endpoint(ec);
    if (ec) return "<closed>";
    return ep.address().to_string() + ":" + std::to_string(ep.port());
}

const ServerConnectionStats& ServerConnection::GetStats() const noexcept
{
    return m_stats;
}

std::chrono::steady_clock::time_point ServerConnection::GetLastReceivedTime() const noexcept
{
    return std::chrono::steady_clock::time_point(
        std::chrono::steady_clock::duration(
            m_lastReceivedTime.load(std::memory_order_relaxed)));
}

} // namespace SagaServer::Networking
