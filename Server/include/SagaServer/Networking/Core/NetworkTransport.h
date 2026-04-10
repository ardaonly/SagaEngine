/// @file NetworkTransport.h
/// @brief Async UDP transport layer built on Boost.Asio with send queue, keepalive, and handshake.
///
/// Layer  : SagaServer / Networking / Core
/// Purpose: Provides the INetworkTransport interface and a production UdpTransport
///          implementation. UdpTransport manages a dedicated IO thread, an async
///          send queue with back-pressure, automatic keepalive probing, and a
///          connect-timeout state machine.
///
/// Threading:
///   - Initialize() spawns a dedicated IO thread for the io_context run loop.
///   - Send() is safe to call from any thread (enqueues into a mutex-guarded queue).
///   - Callbacks fire on the IO thread — callers must not block.

#pragma once

#include "SagaServer/Networking/Core/NetworkTypes.h"
#include "SagaServer/Networking/Core/Packet.h"

#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace SagaEngine::Networking
{

// ─── INetworkTransport ────────────────────────────────────────────────────────

/// Transport-agnostic interface for network communication.
class INetworkTransport
{
public:
    virtual ~INetworkTransport() = default;

    virtual bool Initialize(const NetworkConfig& config) = 0;
    virtual void Shutdown() = 0;

    virtual bool Connect(const NetworkAddress& address) = 0;
    virtual void Disconnect(int reason = 0) = 0;

    virtual bool Send(const uint8_t* data, std::size_t size) = 0;
    virtual void Receive() = 0;

    virtual ConnectionState    GetState()      const = 0;
    virtual NetworkStatistics  GetStatistics() const = 0;

    virtual void SetOnConnected(OnConnectedCallback cb) = 0;
    virtual void SetOnDisconnected(OnDisconnectedCallback cb) = 0;
    virtual void SetOnPacketReceived(OnPacketReceivedCallback cb) = 0;
    virtual void SetOnStateChanged(OnStateChangedCallback cb) = 0;
};

// ─── UdpTransport ─────────────────────────────────────────────────────────────

/// Async UDP transport with dedicated IO thread, send queue, and keepalive.
class UdpTransport final : public INetworkTransport
{
public:
    UdpTransport();
    ~UdpTransport() override;

    UdpTransport(const UdpTransport&)            = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;

    // ── INetworkTransport ─────────────────────────────────────────────────────

    bool Initialize(const NetworkConfig& config) override;
    void Shutdown() override;

    bool Connect(const NetworkAddress& address) override;
    void Disconnect(int reason = 0) override;

    bool Send(const uint8_t* data, std::size_t size) override;
    void Receive() override;

    ConnectionState   GetState() const override      { return m_State; }
    NetworkStatistics GetStatistics() const override;

    void SetOnConnected(OnConnectedCallback cb) override       { m_OnConnected = std::move(cb); }
    void SetOnDisconnected(OnDisconnectedCallback cb) override { m_OnDisconnected = std::move(cb); }
    void SetOnPacketReceived(OnPacketReceivedCallback cb) override { m_OnPacketReceived = std::move(cb); }
    void SetOnStateChanged(OnStateChangedCallback cb) override { m_OnStateChanged = std::move(cb); }

    // ── Extended queries ──────────────────────────────────────────────────────

    [[nodiscard]] NetworkAddress GetRemoteAddress() const { return m_RemoteAddress; }
    [[nodiscard]] NetworkAddress GetLocalAddress()  const;

private:
    // ── Asio runtime ──────────────────────────────────────────────────────────

    asio::io_context                            m_IoContext;
    std::unique_ptr<asio::ip::udp::socket>      m_Socket;
    std::unique_ptr<asio::steady_timer>         m_ConnectTimer;
    std::unique_ptr<asio::steady_timer>         m_KeepAliveTimer;
    std::thread                                 m_IoThread;
    std::atomic<bool>                           m_Running{false};

    // ── Connection state ──────────────────────────────────────────────────────

    ConnectionState  m_State{ConnectionState::Disconnected};
    NetworkConfig    m_Config;
    NetworkAddress   m_RemoteAddress;
    ConnectionId     m_ConnectionId{0};

    // ── Statistics ────────────────────────────────────────────────────────────

    NetworkStatistics     m_Stats;
    mutable std::mutex    m_StatsMutex;

    // ── Send queue ────────────────────────────────────────────────────────────

    struct PendingSend
    {
        std::vector<uint8_t> data;
        uint32_t             timestamp{0};
        uint32_t             retryCount{0};
    };

    std::queue<PendingSend> m_SendQueue;
    mutable std::mutex      m_SendQueueMutex;

    // ── Receive buffer ────────────────────────────────────────────────────────

    std::vector<uint8_t>      m_ReceiveBuffer;
    asio::ip::udp::endpoint   m_ReceiveEndpoint;

    // ── Callbacks ─────────────────────────────────────────────────────────────

    OnConnectedCallback      m_OnConnected;
    OnDisconnectedCallback   m_OnDisconnected;
    OnPacketReceivedCallback m_OnPacketReceived;
    OnStateChangedCallback   m_OnStateChanged;

    // ── Internal methods ──────────────────────────────────────────────────────

    void SetState(ConnectionState newState);
    void StartReceive();
    void HandleReceive(const asio::error_code& ec, std::size_t bytesTransferred);
    void ProcessSendQueue();
    void StartKeepAliveTimer();
    void HandleKeepAliveTimer(const asio::error_code& ec);
    void SendKeepAlive();

    void InvokeOnConnected();
    void InvokeOnDisconnected(int reason);
    void InvokeOnPacketReceived(const uint8_t* data, std::size_t size);
    void InvokeOnStateChanged(ConnectionState newState);
};

// ─── TransportFactory ─────────────────────────────────────────────────────────

/// Create transport instances by protocol type.
class TransportFactory final
{
public:
    [[nodiscard]] static std::unique_ptr<INetworkTransport> Create(bool useUdp = true);
};

} // namespace SagaEngine::Networking
