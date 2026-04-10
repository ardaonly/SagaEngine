/// @file ReliableChannel.h
/// @brief Selective-repeat ARQ protocol with SACK, fast retransmit, and congestion-aware RTT estimation.
///
/// Implements a reliable UDP channel using a sliding-window protocol with:
/// - Selective acknowledgments (SACK) via cumulative ACK + 32-bit received bitmask
/// - Fast retransmit on duplicate ACK detection (3+ duplicates triggers immediate resend)
/// - NACK generation on out-of-order gap detection
/// - Karn's Algorithm for RTT estimation (ignores retransmitted packets)
/// - Adaptive retransmission timeout based on smoothed RTT + variance

#pragma once
#include "NetworkTypes.h"
#include "Packet.h"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

namespace SagaEngine::Networking {

/// @brief Configuration parameters for the reliable channel.
struct ReliableChannelConfig {
    uint32_t windowSize{64};         ///< Maximum unacknowledged packets in flight
    uint32_t maxRetransmissions{5};  ///< Packets exceeding this are dropped and reported lost
    uint32_t rttTimeoutMs{100};      ///< Minimum timeout before retransmission
    float rttTimeoutMultiplier{1.5f};///< Multiplier for adaptive RTO (smoothed RTT * multiplier)
    uint32_t ackFrequency{4};        ///< Send ACK every N received packets
    uint32_t dupAckThreshold{3};     ///< Duplicate ACK count before fast retransmit
};

/// @brief Internal tracking structure for in-flight packets.
struct PendingPacket {
    Packet packet;
    uint32_t sendTime;          ///< Millisecond timestamp of last transmission
    uint32_t retransmitCount;   ///< Number of times this packet has been retransmitted
    bool acknowledged;          ///< True once ACK or max retransmit reached
    bool isRetransmission;      ///< True if this is a retransmit (Karn's Algorithm flag)
};

/// @brief Runtime statistics for monitoring and diagnostics.
struct ChannelStatistics {
    uint64_t packetsSent{0};
    uint64_t packetsAcknowledged{0};
    uint64_t packetsRetransmitted{0};
    uint64_t packetsLost{0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    float averageRttMs{0.0f};
    float currentRttMs{0.0f};
    float jitterMs{0.0f};
    float packetLossRate{0.0f};
    uint64_t fastRetransmits{0};     ///< Count of fast retransmit triggers (dup ACK)
    uint64_t timeoutRetransmits{0};  ///< Count of timeout-based retransmits
    uint64_t nacksSent{0};           ///< NACK packets emitted
    uint64_t sacksReceived{0};       ///< SACK bitMaps processed

    void Reset() {
        packetsSent = 0;
        packetsAcknowledged = 0;
        packetsRetransmitted = 0;
        packetsLost = 0;
        bytesSent = 0;
        bytesReceived = 0;
        averageRttMs = 0.0f;
        currentRttMs = 0.0f;
        jitterMs = 0.0f;
        packetLossRate = 0.0f;
        fastRetransmits = 0;
        timeoutRetransmits = 0;
        nacksSent = 0;
        sacksReceived = 0;
    }
};

/// @brief Reliable UDP channel implementing selective-repeat ARQ.
///
/// Thread-safe, per-connection reliable delivery over an unreliable transport.
/// The caller feeds raw bytes into Receive() and retrieves serialized packets
/// from GetPacketsToSend() for transmission via the underlying transport.
class ReliableChannel {
public:
    explicit ReliableChannel(ConnectionId connectionId);
    ~ReliableChannel() = default;

    ReliableChannel(const ReliableChannel&) = delete;
    ReliableChannel& operator=(const ReliableChannel&) = delete;

    /// @brief Initialize the channel with the given configuration.
    /// @return False if already initialized.
    bool Initialize(const ReliableChannelConfig& config);

    /// @brief Shut down and clear all buffers.
    void Shutdown();

    /// @brief Queue a packet for reliable delivery.
    /// @return False if disconnected or send window is full.
    bool Send(Packet packet);

    /// @brief Process inbound bytes from the transport layer.
    void Receive(const uint8_t* data, size_t size);

    /// @brief Periodic update call (checks retransmissions, sends deferred ACKs).
    /// @param currentTimeMs Current monotonic clock in milliseconds.
    void Update(uint32_t currentTimeMs);

    /// @brief Drain received packets (caller-owned).
    std::vector<Packet> GetReceivedPackets();

    /// @brief Drain packets ready for transport transmission.
    std::vector<Packet> GetPacketsToSend();

    ConnectionId GetConnectionId() const { return m_ConnectionId; }
    bool IsConnected() const { return m_IsConnected; }
    void SetConnected(bool connected) { m_IsConnected = connected; }

    ChannelStatistics GetStatistics() const;
    void ResetStatistics();

    float GetRtt() const { return m_Statistics.currentRttMs; }
    float GetJitter() const { return m_Statistics.jitterMs; }
    float GetPacketLossRate() const { return m_Statistics.packetLossRate; }

    using OnPacketAcknowledgedCallback = std::function<void(uint32_t sequence)>;
    using OnPacketLostCallback = std::function<void(uint32_t sequence)>;
    using OnRttUpdatedCallback = std::function<void(float rttMs)>;

    void SetOnPacketAcknowledged(OnPacketAcknowledgedCallback cb) { m_OnPacketAcknowledged = std::move(cb); }
    void SetOnPacketLost(OnPacketLostCallback cb) { m_OnPacketLost = std::move(cb); }
    void SetOnRttUpdated(OnRttUpdatedCallback cb) { m_OnRttUpdated = std::move(cb); }

private:
    ConnectionId m_ConnectionId;
    ReliableChannelConfig m_Config;
    bool m_IsConnected{false};

    std::atomic<uint32_t> m_NextSendSequence{1};
    std::atomic<uint32_t> m_NextExpectedSequence{1};

    std::deque<PendingPacket> m_SendWindow;
    std::unordered_map<uint32_t, Packet> m_ReassemblyBuffer;
    std::unordered_set<uint32_t> m_ProcessedSequences;
    std::vector<uint32_t> m_ReceivedSequences;
    std::vector<Packet> m_ReceivedPackets;
    std::vector<Packet> m_PacketsToSend;
    std::vector<uint32_t> m_PendingAcks;

    uint32_t m_LastAckSent{0};
    uint32_t m_PacketsSinceLastAck{0};

    ChannelStatistics m_Statistics;
    mutable std::mutex m_Mutex;

    std::vector<float> m_RttSamples;
    float m_SmoothedRtt{0.0f};
    float m_RttVariation{0.0f};
    uint32_t m_LastRttUpdateTime{0};

    // Fast retransmit: track duplicate ACK counts per sequence
    std::unordered_map<uint32_t, uint32_t> m_DupAckCounts;

    // SACK: bitmask of recently received sequences (relative to m_NextExpectedSequence)
    static constexpr uint32_t kSackBitmaskSize = 32;
    uint32_t m_SackBitmask{0};

    OnPacketAcknowledgedCallback m_OnPacketAcknowledged;
    OnPacketLostCallback m_OnPacketLost;
    OnRttUpdatedCallback m_OnRttUpdated;

    void ProcessPacket(const Packet& packet);
    void ProcessAck(uint32_t ackSequence, uint32_t sackBitmask);
    void ProcessNack(uint32_t nackSequence);
    void SendAck();
    void SendNack(uint32_t sequence);
    void CheckRetransmissions(uint32_t currentTimeMs);
    void FastRetransmitPacket(uint32_t sequence);
    void RetransmitPacket(PendingPacket& pending);
    void UpdateRtt(uint32_t sequence, uint32_t rtt, bool isRetransmission);
    void CalculateRttStatistics();
    bool IsSequenceValid(uint32_t sequence) const;
    bool IsSequenceInWindow(uint32_t sequence) const;
    uint32_t BuildSackBitmask() const;
    bool IsSetInSack(uint32_t sequence) const;
    void InvokeOnPacketAcknowledged(uint32_t sequence);
    void InvokeOnPacketLost(uint32_t sequence);
    void InvokeOnRttUpdated(float rttMs);
};

} // namespace SagaEngine::Networking