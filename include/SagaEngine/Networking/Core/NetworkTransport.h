#pragma once
#include "NetworkTypes.h"
#include "Packet.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <atomic>
#include <SagaEngine/Core/Threading/JobSystem.h>

namespace SagaEngine::Networking {

// ============================================================================
// Transport Layer Interface
// ============================================================================
class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;
    
    virtual bool Initialize(const NetworkConfig& config) = 0;
    virtual void Shutdown() = 0;
    
    virtual bool Connect(const NetworkAddress& address) = 0;
    virtual void Disconnect(int reason = 0) = 0;
    
    virtual bool Send(const uint8_t* data, size_t size) = 0;
    virtual void Receive() = 0;
    
    virtual ConnectionState GetState() const = 0;
    virtual NetworkStatistics GetStatistics() const = 0;
    
    // Callbacks
    virtual void SetOnConnected(OnConnectedCallback cb) = 0;
    virtual void SetOnDisconnected(OnDisconnectedCallback cb) = 0;
    virtual void SetOnPacketReceived(OnPacketReceivedCallback cb) = 0;
    virtual void SetOnStateChanged(OnStateChangedCallback cb) = 0;
};

// ============================================================================
// UDP Transport Implementation (Boost.Asio)
// ============================================================================
class UdpTransport : public INetworkTransport {
public:
    UdpTransport();
    ~UdpTransport() override;
    
    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;
    
    // INetworkTransport
    bool Initialize(const NetworkConfig& config) override;
    void Shutdown() override;
    
    bool Connect(const NetworkAddress& address) override;
    void Disconnect(int reason = 0) override;
    
    bool Send(const uint8_t* data, size_t size) override;
    void Receive() override;
    
    ConnectionState GetState() const override { return m_State; }
    NetworkStatistics GetStatistics() const override { return m_Stats; }
    
    // Callbacks
    void SetOnConnected(OnConnectedCallback cb) override { m_OnConnected = std::move(cb); }
    void SetOnDisconnected(OnDisconnectedCallback cb) override { m_OnDisconnected = std::move(cb); }
    void SetOnPacketReceived(OnPacketReceivedCallback cb) override { m_OnPacketReceived = std::move(cb); }
    void SetOnStateChanged(OnStateChangedCallback cb) override { m_OnStateChanged = std::move(cb); }
    
    // Connection Info
    NetworkAddress GetRemoteAddress() const { return m_RemoteAddress; }
    NetworkAddress GetLocalAddress() const;
    
private:
    // Boost.Asio
    boost::asio::io_context m_IoContext;
    std::unique_ptr<boost::asio::ip::udp::socket> m_Socket;
    std::unique_ptr<boost::asio::steady_timer> m_ConnectTimer;
    std::unique_ptr<boost::asio::steady_timer> m_KeepAliveTimer;
    
    // State
    ConnectionState m_State{ConnectionState::Disconnected};
    NetworkConfig m_Config;
    NetworkAddress m_RemoteAddress;
    NetworkAddress m_LocalAddress;
    
    // Statistics
    NetworkStatistics m_Stats;
    mutable std::mutex m_StatsMutex;
    
    // Send Queue (Thread-safe)
    struct PendingSend {
        std::vector<uint8_t> data;
        uint32_t timestamp;
        uint32_t retryCount;
    };
    std::queue<PendingSend> m_SendQueue;
    mutable std::mutex m_SendQueueMutex;
    
    // Receive Buffer
    std::vector<uint8_t> m_ReceiveBuffer;
    boost::asio::ip::udp::endpoint m_ReceiveEndpoint;
    
    // Callbacks
    OnConnectedCallback m_OnConnected;
    OnDisconnectedCallback m_OnDisconnected;
    OnPacketReceivedCallback m_OnPacketReceived;
    OnStateChangedCallback m_OnStateChanged;
    
    // Threading
    std::thread m_IoThread;
    std::atomic<bool> m_Running{false};
    Core::JobHandle m_ProcessJob;
    
    // Internal Methods
    void SetState(ConnectionState newState);
    void StartReceive();
    void HandleReceive(const boost::system::error_code& error, size_t bytesTransferred);
    void ProcessSendQueue();
    void StartConnectTimer();
    void HandleConnectTimer(const boost::system::error_code& error);
    void StartKeepAliveTimer();
    void HandleKeepAliveTimer(const boost::system::error_code& error);
    void SendKeepAlive();
    
    void InvokeOnConnected();
    void InvokeOnDisconnected(int reason);
    void InvokeOnPacketReceived(const uint8_t* data, size_t size);
    void InvokeOnStateChanged(ConnectionState newState);
};

// ============================================================================
// TCP Transport Implementation (For reliable services)
// ============================================================================
class TcpTransport : public INetworkTransport {
public:
    TcpTransport();
    ~TcpTransport() override;
    
    // Similar interface to UdpTransport
    // Implementation would follow the same pattern
    // ... (TCP-specific logic for reliable connections)
    
private:
    // TCP-specific implementation
};

// ============================================================================
// Transport Factory
// ============================================================================
class TransportFactory {
public:
    static std::unique_ptr<INetworkTransport> Create(bool useUdp = true);
};

} // namespace SagaEngine::Networking