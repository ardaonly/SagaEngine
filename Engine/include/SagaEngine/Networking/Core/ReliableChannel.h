#pragma once
#include "NetworkTypes.h"
#include "Packet.h"
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

namespace SagaEngine::Networking {

struct ReliableChannelConfig {
    uint32_t windowSize{64};
    uint32_t maxRetransmissions{5};
    uint32_t rttTimeoutMs{100};
    float rttTimeoutMultiplier{1.5f};
    uint32_t ackFrequency{4};
};

struct PendingPacket {
    Packet packet;
    uint32_t sendTime;
    uint32_t retransmitCount;
    bool acknowledged;
};

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
    }
};

class ReliableChannel {
public:
    explicit ReliableChannel(ConnectionId connectionId);
    ~ReliableChannel() = default;
    
    ReliableChannel(const ReliableChannel&) = delete;
    ReliableChannel& operator=(const ReliableChannel&) = delete;
    
    bool Initialize(const ReliableChannelConfig& config);
    void Shutdown();
    
    bool Send(Packet packet);
    void Receive(const uint8_t* data, size_t size);
    void Update(uint32_t currentTimeMs);
    
    std::vector<Packet> GetReceivedPackets();
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
    
    OnPacketAcknowledgedCallback m_OnPacketAcknowledged;
    OnPacketLostCallback m_OnPacketLost;
    OnRttUpdatedCallback m_OnRttUpdated;
    
    void ProcessPacket(const Packet& packet);
    void ProcessAck(uint32_t ackSequence);
    void ProcessNack(uint32_t nackSequence);
    void SendAck();
    void SendNack(uint32_t sequence);
    void CheckRetransmissions(uint32_t currentTimeMs);
    void RetransmitPacket(PendingPacket& pending);
    void UpdateRtt(uint32_t sequence, uint32_t rtt);
    void CalculateRttStatistics();
    bool IsSequenceValid(uint32_t sequence) const;
    bool IsSequenceInWindow(uint32_t sequence) const;
    void InvokeOnPacketAcknowledged(uint32_t sequence);
    void InvokeOnPacketLost(uint32_t sequence);
    void InvokeOnRttUpdated(float rttMs);
};

} // namespace SagaEngine::Networking