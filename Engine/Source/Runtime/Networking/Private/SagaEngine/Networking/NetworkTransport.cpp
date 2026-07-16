/// @file NetworkTransport.cpp
/// @brief UdpTransport full implementation — Boost.Asio async UDP with send queue and keepalive.

#include "SagaEngine/Networking/NetworkTransport.h"
#include "SagaEngine/Core/Log/Log.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <cassert>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace SagaEngine::Networking
{

using namespace std::chrono_literals;

static constexpr const char* kTag       = "UdpTransport";
static constexpr std::size_t kRecvBufSz = 65536;
static constexpr uint32_t    kKeepaliveIntervalMs = 2000;
static constexpr uint32_t    kConnectTimeoutMs    = 5000;

struct UdpTransport::Impl
{
    struct PendingSend
    {
        std::vector<uint8_t> data;
        uint32_t timestamp{0};
        uint32_t retryCount{0};
    };

    asio::io_context ioContext;
    std::unique_ptr<asio::ip::udp::socket> socket;
    std::unique_ptr<asio::steady_timer> connectTimer;
    std::unique_ptr<asio::steady_timer> keepAliveTimer;
    std::thread ioThread;
    std::atomic<bool> running{false};

    ConnectionState state{ConnectionState::Disconnected};
    NetworkConfig config;
    NetworkAddress remoteAddress;
    ConnectionId connectionId{0};

    NetworkStatistics stats;
    mutable std::mutex statsMutex;

    std::queue<PendingSend> sendQueue;
    mutable std::mutex sendQueueMutex;

    std::vector<uint8_t> receiveBuffer;
    asio::ip::udp::endpoint receiveEndpoint;

    OnConnectedCallback onConnected;
    OnDisconnectedCallback onDisconnected;
    OnPacketReceivedCallback onPacketReceived;
    OnStateChangedCallback onStateChanged;
};

// ─── Construction ─────────────────────────────────────────────────────────────

UdpTransport::UdpTransport()
    : m_Impl(std::make_unique<Impl>())
{
    m_Impl->receiveBuffer.resize(kRecvBufSz);
}

UdpTransport::~UdpTransport()
{
    Shutdown();
}

// ─── Initialize ───────────────────────────────────────────────────────────────

bool UdpTransport::Initialize(const NetworkConfig& config)
{
    m_Impl->config = config;

    asio::error_code ec;

    m_Impl->socket = std::make_unique<asio::ip::udp::socket>(m_Impl->ioContext);
    m_Impl->socket->open(asio::ip::udp::v4(), ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Socket open failed: %s", ec.message().c_str());
        return false;
    }

    m_Impl->socket->set_option(asio::socket_base::reuse_address(true), ec);
    m_Impl->socket->set_option(asio::socket_base::receive_buffer_size(kRecvBufSz), ec);
    m_Impl->socket->set_option(asio::socket_base::send_buffer_size(65536), ec);

    // Bind to any port for client-side usage.
    m_Impl->socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Bind failed: %s", ec.message().c_str());
        return false;
    }

    m_Impl->connectTimer   = std::make_unique<asio::steady_timer>(m_Impl->ioContext);
    m_Impl->keepAliveTimer = std::make_unique<asio::steady_timer>(m_Impl->ioContext);

    m_Impl->running.store(true, std::memory_order_release);

    m_Impl->ioThread = std::thread([this]()
    {
        LOG_DEBUG(kTag, "IO thread started");
        auto workGuard = asio::make_work_guard(m_Impl->ioContext);
        try
        {
            m_Impl->ioContext.run();
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR(kTag, "IO thread exception: %s", ex.what());
        }
        LOG_DEBUG(kTag, "IO thread exited");
    });

    LOG_INFO(kTag, "Initialized — local port %u",
             m_Impl->socket->local_endpoint().port());
    return true;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void UdpTransport::Shutdown()
{
    if (!m_Impl->running.exchange(false, std::memory_order_acq_rel))
        return;

    SetState(ConnectionState::Disconnected);

    asio::error_code ec;
    if (m_Impl->connectTimer)   m_Impl->connectTimer->cancel(ec);
    if (m_Impl->keepAliveTimer) m_Impl->keepAliveTimer->cancel(ec);
    if (m_Impl->socket && m_Impl->socket->is_open())
        m_Impl->socket->close(ec);

    m_Impl->ioContext.stop();

    if (m_Impl->ioThread.joinable())
        m_Impl->ioThread.join();

    LOG_INFO(kTag, "Shutdown complete");
}

// ─── Connect ──────────────────────────────────────────────────────────────────

bool UdpTransport::Connect(const NetworkAddress& address)
{
    if (GetState() != ConnectionState::Disconnected)
    {
        LOG_WARN(kTag, "Connect() called in non-Disconnected state");
        return false;
    }

    asio::error_code ec;
    const asio::ip::address asioAddr = asio::ip::make_address(address.host, ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Invalid address '%s': %s", address.host.c_str(), ec.message().c_str());
        return false;
    }

    m_Impl->remoteAddress = address;
    m_Impl->receiveEndpoint = asio::ip::udp::endpoint(asioAddr, address.port);

    SetState(ConnectionState::Connecting);

    // UDP "connect" sets the default send destination and filters received datagrams.
    m_Impl->socket->connect(m_Impl->receiveEndpoint, ec);
    if (ec)
    {
        LOG_ERROR(kTag, "UDP connect failed: %s", ec.message().c_str());
        SetState(ConnectionState::Disconnected);
        return false;
    }

    // Start a timeout so we don't wait forever for the remote to respond.
    m_Impl->connectTimer->expires_after(std::chrono::milliseconds(kConnectTimeoutMs));
    m_Impl->connectTimer->async_wait([this](const asio::error_code& timerEc)
    {
        if (timerEc) return;
        if (GetState() == ConnectionState::Connecting)
        {
            LOG_WARN(kTag, "Connection timeout — reverting to Disconnected");
            SetState(ConnectionState::Disconnected);
            InvokeOnDisconnected(-1);
        }
    });

    StartReceive();

    // Send a handshake probe so the remote side knows we exist.
    static const uint8_t kHandshakeProbe[] = { 0x53, 0x47, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    Send(kHandshakeProbe, sizeof(kHandshakeProbe));

    LOG_INFO(kTag, "Connecting to %s:%u", address.host.c_str(), address.port);
    return true;
}

// ─── Disconnect ───────────────────────────────────────────────────────────────

void UdpTransport::Disconnect(int reason)
{
    if (GetState() == ConnectionState::Disconnected)
        return;

    asio::error_code ec;
    if (m_Impl->connectTimer)   m_Impl->connectTimer->cancel(ec);
    if (m_Impl->keepAliveTimer) m_Impl->keepAliveTimer->cancel(ec);

    SetState(ConnectionState::Disconnected);
    InvokeOnDisconnected(reason);

    LOG_INFO(kTag, "Disconnected (reason=%d)", reason);
}

// ─── Send ─────────────────────────────────────────────────────────────────────

bool UdpTransport::Send(const uint8_t* data, std::size_t size)
{
    if (!data || size == 0 || size > 65507)
        return false;

    Impl::PendingSend pending;
    pending.data.assign(data, data + size);
    pending.timestamp   = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    pending.retryCount  = 0;

    {
        std::lock_guard<std::mutex> lock(m_Impl->sendQueueMutex);
        m_Impl->sendQueue.push(std::move(pending));
    }

    ProcessSendQueue();
    return true;
}

void UdpTransport::ProcessSendQueue()
{
    Impl::PendingSend pending;
    bool hasPending = false;

    {
        std::lock_guard<std::mutex> lock(m_Impl->sendQueueMutex);
        if (!m_Impl->sendQueue.empty())
        {
            pending    = std::move(m_Impl->sendQueue.front());
            m_Impl->sendQueue.pop();
            hasPending = true;
        }
    }

    if (!hasPending || !m_Impl->socket || !m_Impl->socket->is_open())
        return;

    m_Impl->socket->async_send(
        asio::buffer(pending.data.data(), pending.data.size()),
        [this](const asio::error_code& ec, std::size_t bytes)
        {
            if (!ec)
            {
                std::lock_guard<std::mutex> lock(m_Impl->statsMutex);
                m_Impl->stats.bytesSent    += bytes;
                m_Impl->stats.packetsSent  += 1;
            }
            else if (ec != asio::error::operation_aborted)
            {
                LOG_WARN(kTag, "Send error: %s", ec.message().c_str());
            }

            ProcessSendQueue();
        });
}

// ─── Receive ──────────────────────────────────────────────────────────────────

void UdpTransport::Receive()
{
    // No-op in async mode; StartReceive() is called during Initialize().
}

void UdpTransport::StartReceive()
{
    if (!m_Impl->running.load(std::memory_order_relaxed) ||
        !m_Impl->socket || !m_Impl->socket->is_open())
        return;

    m_Impl->socket->async_receive_from(
        asio::buffer(m_Impl->receiveBuffer.data(), m_Impl->receiveBuffer.size()),
        m_Impl->receiveEndpoint,
        [this](const asio::error_code& ec, std::size_t bytes)
        {
            HandleReceive(ec, bytes);
        });
}

void UdpTransport::HandleReceive(const std::error_code& ec, std::size_t bytesTransferred)
{
    if (!m_Impl->running.load(std::memory_order_relaxed))
        return;

    if (ec)
    {
        if (ec != asio::error::operation_aborted && ec != asio::error::connection_refused)
        {
            LOG_WARN(kTag, "Receive error: %s", ec.message().c_str());
        }
        StartReceive();
        return;
    }

    if (bytesTransferred == 0)
    {
        StartReceive();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_Impl->statsMutex);
        m_Impl->stats.bytesReceived   += bytesTransferred;
        m_Impl->stats.packetsReceived += 1;
    }

    // If we were still Connecting, consider this the handshake response.
    if (GetState() == ConnectionState::Connecting)
    {
        asio::error_code timerEc;
        m_Impl->connectTimer->cancel(timerEc);

        SetState(ConnectionState::Connected);
        InvokeOnConnected();
        StartKeepAliveTimer();
    }

    InvokeOnPacketReceived(m_Impl->receiveBuffer.data(), bytesTransferred);
    StartReceive();
}

// ─── Keepalive ────────────────────────────────────────────────────────────────

void UdpTransport::StartKeepAliveTimer()
{
    if (!m_Impl->running.load(std::memory_order_relaxed))
        return;

    m_Impl->keepAliveTimer->expires_after(std::chrono::milliseconds(kKeepaliveIntervalMs));
    m_Impl->keepAliveTimer->async_wait([this](const asio::error_code& ec)
    {
        HandleKeepAliveTimer(ec);
    });
}

void UdpTransport::HandleKeepAliveTimer(const std::error_code& ec)
{
    if (ec || !m_Impl->running.load(std::memory_order_relaxed))
        return;

    if (GetState() == ConnectionState::Connected)
    {
        SendKeepAlive();
        StartKeepAliveTimer();
    }
}

void UdpTransport::SendKeepAlive()
{
    // Minimal keepalive probe: magic(2) + type=Heartbeat(1) + flags(1) + ts(4)
    const auto nowMs = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    uint8_t probe[8];
    probe[0] = 0x47; probe[1] = 0x53; // magic 'GS'
    probe[2] = 0x01;                  // PacketType::Heartbeat
    probe[3] = 0x00;                  // flags
    std::memcpy(probe + 4, &nowMs, sizeof(nowMs));

    Send(probe, sizeof(probe));

    {
        std::lock_guard<std::mutex> lock(m_Impl->statsMutex);
        m_Impl->stats.heartbeatsSent += 1;
    }
}

// ─── State ────────────────────────────────────────────────────────────────────

void UdpTransport::SetState(ConnectionState newState)
{
    const ConnectionState old = m_Impl->state;
    if (old == newState)
        return;

    m_Impl->state = newState;
    InvokeOnStateChanged(newState);
}

ConnectionState UdpTransport::GetState() const
{
    return m_Impl->state;
}

NetworkAddress UdpTransport::GetRemoteAddress() const
{
    return m_Impl->remoteAddress;
}

NetworkAddress UdpTransport::GetLocalAddress() const
{
    if (!m_Impl->socket)
        return NetworkAddress{};

    asio::error_code ec;
    const auto ep = m_Impl->socket->local_endpoint(ec);
    if (ec)
        return NetworkAddress{};

    NetworkAddress addr;
    addr.host = ep.address().to_string();
    addr.port = ep.port();
    return addr;
}

void UdpTransport::SetOnConnected(OnConnectedCallback cb)
{
    m_Impl->onConnected = std::move(cb);
}

void UdpTransport::SetOnDisconnected(OnDisconnectedCallback cb)
{
    m_Impl->onDisconnected = std::move(cb);
}

void UdpTransport::SetOnPacketReceived(OnPacketReceivedCallback cb)
{
    m_Impl->onPacketReceived = std::move(cb);
}

void UdpTransport::SetOnStateChanged(OnStateChangedCallback cb)
{
    m_Impl->onStateChanged = std::move(cb);
}

// ─── Callback invocation ──────────────────────────────────────────────────────

void UdpTransport::InvokeOnConnected()
{
    if (m_Impl->onConnected) m_Impl->onConnected(m_Impl->connectionId);
}

void UdpTransport::InvokeOnDisconnected(int reason)
{
    if (m_Impl->onDisconnected) m_Impl->onDisconnected(m_Impl->connectionId, reason);
}

void UdpTransport::InvokeOnPacketReceived(const uint8_t* data, std::size_t size)
{
    if (m_Impl->onPacketReceived) m_Impl->onPacketReceived(m_Impl->connectionId, data, size);
}

void UdpTransport::InvokeOnStateChanged(ConnectionState newState)
{
    if (m_Impl->onStateChanged) m_Impl->onStateChanged(m_Impl->connectionId, newState);
}

// ─── Statistics ───────────────────────────────────────────────────────────────

NetworkStatistics UdpTransport::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(m_Impl->statsMutex);
    return m_Impl->stats;
}

// ─── TransportFactory ─────────────────────────────────────────────────────────

std::unique_ptr<INetworkTransport> TransportFactory::CreateUdp()
{
    return std::make_unique<UdpTransport>();
}

} // namespace SagaEngine::Networking
