/// @file NetworkTransport.h
/// @brief Async UDP transport layer built on Boost.Asio with send queue, keepalive, and handshake.
///
/// Layer  : SagaEngine / Networking
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

#include "SagaEngine/Networking/NetworkTypes.h"
#include "SagaEngine/Networking/Packet.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <system_error>

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

    ConnectionState   GetState() const override;
    NetworkStatistics GetStatistics() const override;

    void SetOnConnected(OnConnectedCallback cb) override;
    void SetOnDisconnected(OnDisconnectedCallback cb) override;
    void SetOnPacketReceived(OnPacketReceivedCallback cb) override;
    void SetOnStateChanged(OnStateChangedCallback cb) override;

    // ── Extended queries ──────────────────────────────────────────────────────

    [[nodiscard]] NetworkAddress GetRemoteAddress() const;
    [[nodiscard]] NetworkAddress GetLocalAddress()  const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

    // ── Internal methods ──────────────────────────────────────────────────────

    void SetState(ConnectionState newState);
    void StartReceive();
    void HandleReceive(const std::error_code& ec, std::size_t bytesTransferred);
    void ProcessSendQueue();
    void StartKeepAliveTimer();
    void HandleKeepAliveTimer(const std::error_code& ec);
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
