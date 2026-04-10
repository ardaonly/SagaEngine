#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <SagaEngine/Core/Log/Log.h>

namespace SagaEngine::Networking {

// ============================================================================
// Connection Identifiers
// ============================================================================
using ConnectionId = uint64_t;
using ClientId = uint64_t;
using SessionId = uint64_t;
constexpr ConnectionId INVALID_CONNECTION_ID = UINT64_MAX;
constexpr ClientId INVALID_CLIENT_ID = UINT64_MAX;

// ============================================================================
// Network Addresses
// ============================================================================
struct NetworkAddress {
    std::string host;
    uint16_t port;
    
    NetworkAddress() : port(0) {}
    NetworkAddress(const std::string& h, uint16_t p) : host(h), port(p) {}
    
    std::string ToString() const {
        return host + ":" + std::to_string(port);
    }
    
    bool operator==(const NetworkAddress& other) const {
        return host == other.host && port == other.port;
    }
    
    bool operator!=(const NetworkAddress& other) const {
        return !(*this == other);
    }
};

// ============================================================================
// Connection States
// ============================================================================
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Failed,
    Timeout
};

inline const char* ConnectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Connecting:    return "Connecting";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::Disconnecting: return "Disconnecting";
        case ConnectionState::Failed:        return "Failed";
        case ConnectionState::Timeout:       return "Timeout";
        default:                             return "Unknown";
    }
}

// ============================================================================
// Packet Types (Protocol Definition)
// ============================================================================
enum class PacketType : uint16_t {
    // Connection Management
    HandshakeRequest      = 1,
    HandshakeResponse     = 2,
    Disconnect            = 3,
    KeepAlive             = 4,
    
    // Reliability Layer
    Ack                   = 10,
    Nack                  = 11,
    
    // Replication
    EntitySpawn           = 20,
    EntityDespawn         = 21,
    ComponentUpdate       = 22,
    ComponentRemove       = 23,
    Snapshot              = 24,
    DeltaSnapshot         = 25,
    
    // RPC
    RPCRequest            = 30,
    RPCResponse           = 31,
    RPCEvent              = 32,
    
    // Input
    InputCommand          = 40,
    InputAck              = 41,
    
    // Authority
    AuthorityTransfer     = 50,
    AuthorityRequest      = 51,
    
    // Custom/Game Specific
    Custom                = 100,
    
    // Invalid
    Invalid               = UINT16_MAX
};

inline const char* PacketTypeToString(PacketType type) {
    switch (type) {
        case PacketType::HandshakeRequest:   return "HandshakeRequest";
        case PacketType::HandshakeResponse:  return "HandshakeResponse";
        case PacketType::Disconnect:         return "Disconnect";
        case PacketType::KeepAlive:          return "KeepAlive";
        case PacketType::Ack:                return "Ack";
        case PacketType::Nack:               return "Nack";
        case PacketType::EntitySpawn:        return "EntitySpawn";
        case PacketType::EntityDespawn:      return "EntityDespawn";
        case PacketType::ComponentUpdate:    return "ComponentUpdate";
        case PacketType::Snapshot:           return "Snapshot";
        case PacketType::RPCRequest:         return "RPCRequest";
        case PacketType::RPCResponse:        return "RPCResponse";
        case PacketType::InputCommand:       return "InputCommand";
        case PacketType::AuthorityTransfer:  return "AuthorityTransfer";
        default:                             return "Custom";
    }
}

// ============================================================================
// Network Statistics
// ============================================================================
struct NetworkStatistics {
    uint64_t packetsSent{0};
    uint64_t packetsReceived{0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    uint64_t packetsLost{0};
    uint64_t packetsRetransmitted{0};
    uint64_t heartbeatsSent{0};
    float averageLatencyMs{0.0f};
    float jitterMs{0.0f};
    float packetLossRate{0.0f};

    void Reset() {
        packetsSent = 0;
        packetsReceived = 0;
        bytesSent = 0;
        bytesReceived = 0;
        packetsLost = 0;
        packetsRetransmitted = 0;
        heartbeatsSent = 0;
        averageLatencyMs = 0.0f;
        jitterMs = 0.0f;
        packetLossRate = 0.0f;
    }
    
    std::string ToString() const {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
            "NetworkStats: Sent=%llu/%llu bytes, Recv=%llu/%llu bytes, "
            "Loss=%.2f%%, Latency=%.2fms, Jitter=%.2fms",
            packetsSent, bytesSent,
            packetsReceived, bytesReceived,
            packetLossRate * 100.0f,
            averageLatencyMs,
            jitterMs);
        return std::string(buffer);
    }
};

// ============================================================================
// Network Configuration
// ============================================================================
struct NetworkConfig {
    // Connection
    uint32_t connectTimeoutMs{5000};
    uint32_t disconnectTimeoutMs{3000};
    uint32_t keepAliveIntervalMs{5000};
    uint32_t maxConnectionAttempts{3};
    
    // Packet
    uint16_t maxPacketSize{1400};  // MTU-safe
    uint16_t maxPacketsPerFrame{100};
    
    // Reliability
    uint32_t maxRetransmissions{5};
    uint32_t retransmissionTimeoutMs{100};
    float retransmissionTimeoutMultiplier{1.5f};
    
    // Bandwidth
    uint32_t maxBandwidthBps{1000000};  // 1 Mbps default
    uint32_t sendRateLimit{60};  // Packets per second
    
    // Security
    bool enableEncryption{true};
    bool enableCompression{true};
    
    NetworkConfig() = default;
};

// ============================================================================
// Callbacks
// ============================================================================
using OnConnectedCallback = std::function<void(ConnectionId)>;
using OnDisconnectedCallback = std::function<void(ConnectionId, int reason)>;
using OnPacketReceivedCallback = std::function<void(ConnectionId, const uint8_t* data, size_t size)>;
using OnStateChangedCallback = std::function<void(ConnectionId, ConnectionState)>;
using OnStatisticsCallback = std::function<void(const NetworkStatistics&)>;

} // namespace SagaEngine::Networking