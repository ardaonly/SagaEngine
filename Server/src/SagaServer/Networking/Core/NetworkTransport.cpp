#include "SagaServer/Networking/Core/NetworkTransport.h"
#include "SagaEngine/Core/Time/Time.h"
#include <chrono>
#include <thread>

namespace SagaEngine::Networking {

UdpTransport::UdpTransport()
    : m_IoContext(1)
    , m_ReceiveBuffer(2048) {
}

UdpTransport::~UdpTransport() {
    Shutdown();
}

bool UdpTransport::Initialize(const NetworkConfig& config) {
    if (m_Running.load()) {
        LOG_WARN("UdpTransport", "Already initialized");
        return false;
    }
    
    m_Config = config;
    
    try {
        m_Socket = std::make_unique<asio::ip::udp::socket>(m_IoContext);
        m_Socket->open(asio::ip::udp::v4());
        
        asio::ip::udp::socket::receive_buffer_size recvSize(256 * 1024);
        asio::ip::udp::socket::send_buffer_size sendSize(256 * 1024);
        m_Socket->set_option(recvSize);
        m_Socket->set_option(sendSize);
        
        asio::ip::udp::endpoint localEndpoint(asio::ip::udp::v4(), 0);
        m_Socket->bind(localEndpoint);
        
        auto localEp = m_Socket->local_endpoint();
        m_LocalAddress.host = localEp.address().to_string();
        m_LocalAddress.port = localEp.port();
        
        LOG_INFO("UdpTransport", "Initialized on %s", m_LocalAddress.ToString().c_str());
        
        m_Running.store(true);
        m_IoThread = std::thread([this]() {
            m_IoContext.run();
        });
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("UdpTransport", "Initialization failed: %s", e.what());
        return false;
    }
}

void UdpTransport::Shutdown() {
    if (!m_Running.load()) {
        return;
    }
    
    LOG_INFO("UdpTransport", "Shutting down...");
    
    Disconnect(0);
    
    m_Running.store(false);
    m_IoContext.stop();
    
    if (m_IoThread.joinable()) {
        m_IoThread.join();
    }
    
    m_Socket.reset();
    m_ConnectTimer.reset();
    m_KeepAliveTimer.reset();
    
    LOG_INFO("UdpTransport", "Shutdown complete");
}

bool UdpTransport::Connect(const NetworkAddress& address) {
    if (m_State != ConnectionState::Disconnected) {
        LOG_WARN("UdpTransport", "Cannot connect - current state: %s",
                 ConnectionStateToString(m_State));
        return false;
    }
    
    m_RemoteAddress = address;
    
    try {
        asio::ip::udp::resolver resolver(m_IoContext);
        auto results = resolver.resolve(asio::ip::udp::v4(),
                                        address.host,
                                        std::to_string(address.port));
        
        if (results.empty()) {
            LOG_ERROR("UdpTransport", "Failed to resolve %s", address.ToString().c_str());
            SetState(ConnectionState::Failed);
            return false;
        }
        
        auto endpoint = *results.begin();
        m_Socket->connect(endpoint);
        
        LOG_INFO("UdpTransport", "Connected to %s", address.ToString().c_str());
        
        SetState(ConnectionState::Connected);
        StartReceive();
        StartKeepAliveTimer();
        
        InvokeOnConnected();
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("UdpTransport", "Connection failed: %s", e.what());
        SetState(ConnectionState::Failed);
        InvokeOnDisconnected(1);
        return false;
    }
}

void UdpTransport::Disconnect(int reason) {
    if (m_State == ConnectionState::Disconnected) {
        return;
    }
    
    LOG_INFO("UdpTransport", "Disconnecting (reason: %d)", reason);
    
    SetState(ConnectionState::Disconnecting);
    
    if (m_Socket && m_Socket->is_open() && m_State == ConnectionState::Connected) {
        Packet disconnectPacket(PacketType::Disconnect);
        disconnectPacket.Write(reason);
        Send(disconnectPacket.GetData(), disconnectPacket.GetTotalSize());
    }
    
    if (m_Socket && m_Socket->is_open()) {
        std::error_code ec;
        m_Socket->shutdown(asio::ip::udp::socket::shutdown_both, ec);
        m_Socket->close(ec);
    }
    
    SetState(ConnectionState::Disconnected);
    InvokeOnDisconnected(reason);
}

bool UdpTransport::Send(const uint8_t* data, size_t size) {
    if (m_State != ConnectionState::Connected) {
        return false;
    }
    
    if (!m_Socket || !m_Socket->is_open()) {
        return false;
    }
    
    try {
        {
            std::lock_guard<std::mutex> lock(m_SendQueueMutex);
            PendingSend pending;
            pending.data.assign(data, data + size);
            pending.timestamp = static_cast<uint32_t>(
                Core::Time::GetTime() * 1000.0);
            pending.retryCount = 0;
            m_SendQueue.push(std::move(pending));
        }
        
        ProcessSendQueue();
        
        {
            std::lock_guard<std::mutex> lock(m_StatsMutex);
            m_Stats.packetsSent++;
            m_Stats.bytesSent += size;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("UdpTransport", "Send failed: %s", e.what());
        return false;
    }
}

void UdpTransport::Receive() {
    StartReceive();
}

void UdpTransport::SetState(ConnectionState newState) {
    if (m_State == newState) {
        return;
    }
    
    ConnectionState oldState = m_State;
    m_State = newState;
    
    LOG_INFO("UdpTransport", "State changed: %s -> %s",
             ConnectionStateToString(oldState),
             ConnectionStateToString(newState));
    
    InvokeOnStateChanged(newState);
}

void UdpTransport::StartReceive() {
    if (!m_Socket || !m_Socket->is_open()) {
        return;
    }
    
    m_Socket->async_receive_from(
        asio::buffer(m_ReceiveBuffer),
        m_ReceiveEndpoint,
        [this](const std::error_code& error, size_t bytesTransferred) {
            HandleReceive(error, bytesTransferred);
        });
}

void UdpTransport::HandleReceive(const std::error_code& error,
                                  size_t bytesTransferred) {
    if (error || bytesTransferred == 0) {
        if (error != asio::error::operation_aborted) {
            LOG_WARN("UdpTransport", "Receive error: %s", error.message().c_str());
        }
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_StatsMutex);
        m_Stats.packetsReceived++;
        m_Stats.bytesReceived += bytesTransferred;
    }
    
    InvokeOnPacketReceived(m_ReceiveBuffer.data(), bytesTransferred);
    
    StartReceive();
}

void UdpTransport::ProcessSendQueue() {
    std::lock_guard<std::mutex> lock(m_SendQueueMutex);
    
    while (!m_SendQueue.empty() && m_Socket && m_Socket->is_open()) {
        auto& pending = m_SendQueue.front();
        
        try {
            m_Socket->send(asio::buffer(pending.data));
            m_SendQueue.pop();
        }
        catch (const std::exception& e) {
            LOG_ERROR("UdpTransport", "Send queue error: %s", e.what());
            break;
        }
    }
}

void UdpTransport::StartKeepAliveTimer() {
    if (!m_Config.keepAliveIntervalMs) {
        return;
    }
    
    m_KeepAliveTimer = std::make_unique<asio::steady_timer>(m_IoContext);
    
    auto interval = std::chrono::milliseconds(m_Config.keepAliveIntervalMs);
    m_KeepAliveTimer->expires_after(interval);
    m_KeepAliveTimer->async_wait(
        [this](const std::error_code& error) {
            HandleKeepAliveTimer(error);
        });
}

void UdpTransport::HandleKeepAliveTimer(const std::error_code& error) {
    if (error || m_State != ConnectionState::Connected) {
        return;
    }
    
    SendKeepAlive();
    StartKeepAliveTimer();
}

void UdpTransport::SendKeepAlive() {
    Packet keepAlive(PacketType::KeepAlive);
    uint32_t timestamp = static_cast<uint32_t>(Core::Time::GetTime() * 1000.0);
    keepAlive.Write(timestamp);
    Send(keepAlive.GetData(), keepAlive.GetTotalSize());
}

NetworkAddress UdpTransport::GetLocalAddress() const {
    return m_LocalAddress;
}

void UdpTransport::InvokeOnConnected() {
    if (m_OnConnected) {
        m_OnConnected(0);
    }
}

void UdpTransport::InvokeOnDisconnected(int reason) {
    if (m_OnDisconnected) {
        m_OnDisconnected(0, reason);
    }
}

void UdpTransport::InvokeOnPacketReceived(const uint8_t* data, size_t size) {
    if (m_OnPacketReceived) {
        m_OnPacketReceived(0, data, size);
    }
}

void UdpTransport::InvokeOnStateChanged(ConnectionState newState) {
    if (m_OnStateChanged) {
        m_OnStateChanged(0, newState);
    }
}

std::unique_ptr<INetworkTransport> TransportFactory::Create(bool useUdp) {
    if (useUdp) {
        return std::make_unique<UdpTransport>();
    }
    return nullptr;
}

} // namespace SagaEngine::Networking