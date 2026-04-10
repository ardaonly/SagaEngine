/// @file NetworkTransport.cpp
/// @brief UdpTransport full implementation — Boost.Asio async UDP with send queue and keepalive.

#include "SagaServer/Networking/Core/NetworkTransport.h"
#include "SagaEngine/Core/Log/Log.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <cassert>
#include <chrono>
#include <cstring>

namespace SagaEngine::Networking
{

using namespace std::chrono_literals;

static constexpr const char* kTag       = "UdpTransport";
static constexpr std::size_t kRecvBufSz = 65536;
static constexpr uint32_t    kKeepaliveIntervalMs = 2000;
static constexpr uint32_t    kConnectTimeoutMs    = 5000;

// ─── Construction ─────────────────────────────────────────────────────────────

UdpTransport::UdpTransport()
    : m_ReceiveBuffer(kRecvBufSz)
{
}

UdpTransport::~UdpTransport()
{
    Shutdown();
}

// ─── Initialize ───────────────────────────────────────────────────────────────

bool UdpTransport::Initialize(const NetworkConfig& config)
{
    m_Config = config;

    asio::error_code ec;

    m_Socket = std::make_unique<asio::ip::udp::socket>(m_IoContext);
    m_Socket->open(asio::ip::udp::v4(), ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Socket open failed: %s", ec.message().c_str());
        return false;
    }

    m_Socket->set_option(asio::socket_base::reuse_address(true), ec);
    m_Socket->set_option(asio::socket_base::receive_buffer_size(kRecvBufSz), ec);
    m_Socket->set_option(asio::socket_base::send_buffer_size(65536), ec);

    // Bind to any port for client-side usage.
    m_Socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
    if (ec)
    {
        LOG_ERROR(kTag, "Bind failed: %s", ec.message().c_str());
        return false;
    }

    m_ConnectTimer   = std::make_unique<asio::steady_timer>(m_IoContext);
    m_KeepAliveTimer = std::make_unique<asio::steady_timer>(m_IoContext);

    m_Running.store(true, std::memory_order_release);

    m_IoThread = std::thread([this]()
    {
        LOG_DEBUG(kTag, "IO thread started");
        auto workGuard = asio::make_work_guard(m_IoContext);
        try
        {
            m_IoContext.run();
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR(kTag, "IO thread exception: %s", ex.what());
        }
        LOG_DEBUG(kTag, "IO thread exited");
    });

    LOG_INFO(kTag, "Initialized — local port %u",
             m_Socket->local_endpoint().port());
    return true;
}

// ─── Shutdown ─────────────────────────────────────────────────────────────────

void UdpTransport::Shutdown()
{
    if (!m_Running.exchange(false, std::memory_order_acq_rel))
        return;

    SetState(ConnectionState::Disconnected);

    asio::error_code ec;
    if (m_ConnectTimer)   m_ConnectTimer->cancel(ec);
    if (m_KeepAliveTimer) m_KeepAliveTimer->cancel(ec);
    if (m_Socket && m_Socket->is_open())
        m_Socket->close(ec);

    m_IoContext.stop();

    if (m_IoThread.joinable())
        m_IoThread.join();

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

    m_RemoteAddress = address;
    m_ReceiveEndpoint = asio::ip::udp::endpoint(asioAddr, address.port);

    SetState(ConnectionState::Connecting);

    // UDP "connect" sets the default send destination and filters received datagrams.
    m_Socket->connect(m_ReceiveEndpoint, ec);
    if (ec)
    {
        LOG_ERROR(kTag, "UDP connect failed: %s", ec.message().c_str());
        SetState(ConnectionState::Disconnected);
        return false;
    }

    // Start a timeout so we don't wait forever for the remote to respond.
    m_ConnectTimer->expires_after(std::chrono::milliseconds(kConnectTimeoutMs));
    m_ConnectTimer->async_wait([this](const asio::error_code& timerEc)
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
    if (m_ConnectTimer)   m_ConnectTimer->cancel(ec);
    if (m_KeepAliveTimer) m_KeepAliveTimer->cancel(ec);

    SetState(ConnectionState::Disconnected);
    InvokeOnDisconnected(reason);

    LOG_INFO(kTag, "Disconnected (reason=%d)", reason);
}

// ─── Send ─────────────────────────────────────────────────────────────────────

bool UdpTransport::Send(const uint8_t* data, std::size_t size)
{
    if (!data || size == 0 || size > 65507)
        return false;

    PendingSend pending;
    pending.data.assign(data, data + size);
    pending.timestamp   = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    pending.retryCount  = 0;

    {
        std::lock_guard<std::mutex> lock(m_SendQueueMutex);
        m_SendQueue.push(std::move(pending));
    }

    ProcessSendQueue();
    return true;
}

void UdpTransport::ProcessSendQueue()
{
    PendingSend pending;
    bool hasPending = false;

    {
        std::lock_guard<std::mutex> lock(m_SendQueueMutex);
        if (!m_SendQueue.empty())
        {
            pending    = std::move(m_SendQueue.front());
            m_SendQueue.pop();
            hasPending = true;
        }
    }

    if (!hasPending || !m_Socket || !m_Socket->is_open())
        return;

    m_Socket->async_send(
        asio::buffer(pending.data.data(), pending.data.size()),
        [this](const asio::error_code& ec, std::size_t bytes)
        {
            if (!ec)
            {
                std::lock_guard<std::mutex> lock(m_StatsMutex);
                m_Stats.bytesSent    += bytes;
                m_Stats.packetsSent  += 1;
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
    if (!m_Running.load(std::memory_order_relaxed) ||
        !m_Socket || !m_Socket->is_open())
        return;

    m_Socket->async_receive_from(
        asio::buffer(m_ReceiveBuffer.data(), m_ReceiveBuffer.size()),
        m_ReceiveEndpoint,
        [this](const asio::error_code& ec, std::size_t bytes)
        {
            HandleReceive(ec, bytes);
        });
}

void UdpTransport::HandleReceive(const asio::error_code& ec, std::size_t bytesTransferred)
{
    if (!m_Running.load(std::memory_order_relaxed))
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
        std::lock_guard<std::mutex> lock(m_StatsMutex);
        m_Stats.bytesReceived   += bytesTransferred;
        m_Stats.packetsReceived += 1;
    }

    // If we were still Connecting, consider this the handshake response.
    if (GetState() == ConnectionState::Connecting)
    {
        asio::error_code timerEc;
        m_ConnectTimer->cancel(timerEc);

        SetState(ConnectionState::Connected);
        InvokeOnConnected();
        StartKeepAliveTimer();
    }

    InvokeOnPacketReceived(m_ReceiveBuffer.data(), bytesTransferred);
    StartReceive();
}

// ─── Keepalive ────────────────────────────────────────────────────────────────

void UdpTransport::StartKeepAliveTimer()
{
    if (!m_Running.load(std::memory_order_relaxed))
        return;

    m_KeepAliveTimer->expires_after(std::chrono::milliseconds(kKeepaliveIntervalMs));
    m_KeepAliveTimer->async_wait([this](const asio::error_code& ec)
    {
        HandleKeepAliveTimer(ec);
    });
}

void UdpTransport::HandleKeepAliveTimer(const asio::error_code& ec)
{
    if (ec || !m_Running.load(std::memory_order_relaxed))
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
        std::lock_guard<std::mutex> lock(m_StatsMutex);
        m_Stats.heartbeatsSent += 1;
    }
}

// ─── State ────────────────────────────────────────────────────────────────────

void UdpTransport::SetState(ConnectionState newState)
{
    const ConnectionState old = m_State;
    if (old == newState)
        return;

    m_State = newState;
    InvokeOnStateChanged(newState);
}

NetworkAddress UdpTransport::GetLocalAddress() const
{
    asio::error_code ec;
    const auto ep = m_Socket->local_endpoint(ec);
    if (ec)
        return NetworkAddress{};

    NetworkAddress addr;
    addr.host = ep.address().to_string();
    addr.port = ep.port();
    return addr;
}

// ─── Callback invocation ──────────────────────────────────────────────────────

void UdpTransport::InvokeOnConnected()
{
    if (m_OnConnected) m_OnConnected(m_ConnectionId);
}

void UdpTransport::InvokeOnDisconnected(int reason)
{
    if (m_OnDisconnected) m_OnDisconnected(m_ConnectionId, reason);
}

void UdpTransport::InvokeOnPacketReceived(const uint8_t* data, std::size_t size)
{
    if (m_OnPacketReceived) m_OnPacketReceived(m_ConnectionId, data, size);
}

void UdpTransport::InvokeOnStateChanged(ConnectionState newState)
{
    if (m_OnStateChanged) m_OnStateChanged(m_ConnectionId, newState);
}

// ─── Statistics ───────────────────────────────────────────────────────────────

NetworkStatistics UdpTransport::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(m_StatsMutex);
    return m_Stats;
}

// ─── TransportFactory ─────────────────────────────────────────────────────────

std::unique_ptr<INetworkTransport> TransportFactory::Create(bool useUdp)
{
    if (useUdp)
        return std::make_unique<UdpTransport>();

    // TCP transport can be added here.
    LOG_ERROR("TransportFactory", "TCP transport not implemented — returning null");
    return nullptr;
}

} // namespace SagaEngine::Networking
