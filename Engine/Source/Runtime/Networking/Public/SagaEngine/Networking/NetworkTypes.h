#pragma once

/// @file NetworkTypes.h
/// @brief Engine-owned neutral networking identifiers and protocol enums.

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace SagaEngine::Networking {

using ConnectionId = std::uint64_t;
using ClientId = std::uint64_t;
using SessionId = std::uint64_t;

inline constexpr ConnectionId INVALID_CONNECTION_ID = UINT64_MAX;
inline constexpr ClientId INVALID_CLIENT_ID = UINT64_MAX;

struct NetworkAddress {
    std::string host;
    std::uint16_t port = 0;

    NetworkAddress() = default;
    NetworkAddress(const std::string& h, std::uint16_t p)
        : host(h)
        , port(p)
    {
    }

    [[nodiscard]] std::string ToString() const
    {
        return host + ":" + std::to_string(port);
    }

    [[nodiscard]] bool operator==(const NetworkAddress& other) const
    {
        return host == other.host && port == other.port;
    }

    [[nodiscard]] bool operator!=(const NetworkAddress& other) const
    {
        return !(*this == other);
    }
};

enum class ConnectionState : std::uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Failed,
    Timeout
};

inline const char* ConnectionStateToString(ConnectionState state)
{
    switch (state)
    {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Connecting:    return "Connecting";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::Disconnecting: return "Disconnecting";
        case ConnectionState::Failed:        return "Failed";
        case ConnectionState::Timeout:       return "Timeout";
        default:                             return "Unknown";
    }
}

enum class PacketType : std::uint16_t {
    HandshakeRequest      = 1,
    HandshakeResponse     = 2,
    Disconnect            = 3,
    KeepAlive             = 4,

    Ack                   = 10,
    Nack                  = 11,

    EntitySpawn           = 20,
    EntityDespawn         = 21,
    ComponentUpdate       = 22,
    ComponentRemove       = 23,
    Snapshot              = 24,
    DeltaSnapshot         = 25,

    RPCRequest            = 30,
    RPCResponse           = 31,
    RPCEvent              = 32,

    InputCommand          = 40,
    InputAck              = 41,

    AuthorityTransfer     = 50,
    AuthorityRequest      = 51,

    Custom                = 100,
    Invalid               = UINT16_MAX
};

inline const char* PacketTypeToString(PacketType type)
{
    switch (type)
    {
        case PacketType::HandshakeRequest:   return "HandshakeRequest";
        case PacketType::HandshakeResponse:  return "HandshakeResponse";
        case PacketType::Disconnect:         return "Disconnect";
        case PacketType::KeepAlive:          return "KeepAlive";
        case PacketType::Ack:                return "Ack";
        case PacketType::Nack:               return "Nack";
        case PacketType::EntitySpawn:        return "EntitySpawn";
        case PacketType::EntityDespawn:      return "EntityDespawn";
        case PacketType::ComponentUpdate:    return "ComponentUpdate";
        case PacketType::ComponentRemove:    return "ComponentRemove";
        case PacketType::Snapshot:           return "Snapshot";
        case PacketType::DeltaSnapshot:      return "DeltaSnapshot";
        case PacketType::RPCRequest:         return "RPCRequest";
        case PacketType::RPCResponse:        return "RPCResponse";
        case PacketType::RPCEvent:           return "RPCEvent";
        case PacketType::InputCommand:       return "InputCommand";
        case PacketType::InputAck:           return "InputAck";
        case PacketType::AuthorityTransfer:  return "AuthorityTransfer";
        case PacketType::AuthorityRequest:   return "AuthorityRequest";
        case PacketType::Custom:             return "Custom";
        case PacketType::Invalid:            return "Invalid";
        default:                             return "Unknown";
    }
}

struct NetworkStatistics {
    std::uint64_t packetsSent{0};
    std::uint64_t packetsReceived{0};
    std::uint64_t bytesSent{0};
    std::uint64_t bytesReceived{0};
    std::uint64_t packetsLost{0};
    std::uint64_t packetsRetransmitted{0};
    std::uint64_t heartbeatsSent{0};
    float averageLatencyMs{0.0f};
    float jitterMs{0.0f};
    float packetLossRate{0.0f};

    void Reset()
    {
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

    [[nodiscard]] std::string ToString() const;
};

struct NetworkConfig {
    std::uint32_t connectTimeoutMs{5000};
    std::uint32_t disconnectTimeoutMs{3000};
    std::uint32_t keepAliveIntervalMs{5000};
    std::uint32_t maxConnectionAttempts{3};

    std::uint16_t maxPacketSize{1400};
    std::uint16_t maxPacketsPerFrame{100};

    std::uint32_t maxRetransmissions{5};
    std::uint32_t retransmissionTimeoutMs{100};
    float retransmissionTimeoutMultiplier{1.5f};

    std::uint32_t maxBandwidthBps{1000000};
    std::uint32_t sendRateLimit{60};

    bool enableEncryption{true};
    bool enableCompression{true};
};

using OnConnectedCallback = std::function<void(ConnectionId)>;
using OnDisconnectedCallback = std::function<void(ConnectionId, int reason)>;
using OnPacketReceivedCallback = std::function<void(ConnectionId, const std::uint8_t* data, std::size_t size)>;
using OnStateChangedCallback = std::function<void(ConnectionId, ConnectionState)>;
using OnStatisticsCallback = std::function<void(const NetworkStatistics&)>;

} // namespace SagaEngine::Networking
