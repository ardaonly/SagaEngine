#include "SagaServer/Networking/Core/ReliableChannel.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>

namespace SagaEngine::Networking {

constexpr float SMOOTHING_FACTOR = 0.125f;
constexpr float VARIATION_FACTOR = 0.25f;
constexpr size_t MAX_RTT_SAMPLES = 100;
constexpr size_t MAX_PROCESSED_HISTORY = 1024;

ReliableChannel::ReliableChannel(ConnectionId connectionId)
    : m_ConnectionId(connectionId) {
}

bool ReliableChannel::Initialize(const ReliableChannelConfig& config) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_IsConnected) {
        LOG_WARN("ReliableChannel", "Channel already initialized");
        return false;
    }
    m_Config = config;
    m_SendWindow.clear();
    m_ReassemblyBuffer.clear();
    m_ReceivedPackets.clear();
    m_PacketsToSend.clear();
    m_PendingAcks.clear();
    m_ProcessedSequences.clear();
    m_RttSamples.clear();
    m_NextSendSequence.store(1, std::memory_order_relaxed);
    m_NextExpectedSequence.store(1, std::memory_order_relaxed);
    m_SmoothedRtt = 0.0f;
    m_RttVariation = 0.0f;
    m_LastRttUpdateTime = 0;
    m_Statistics.Reset();
    LOG_INFO("ReliableChannel", "Initialized for connection %llu", m_ConnectionId);
    return true;
}

void ReliableChannel::Shutdown() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    LOG_INFO("ReliableChannel", "Shutting down (sent=%llu, acked=%llu, lost=%llu)",
        m_Statistics.packetsSent,
        m_Statistics.packetsAcknowledged,
        m_Statistics.packetsLost);
    m_IsConnected = false;
    m_SendWindow.clear();
    m_ReassemblyBuffer.clear();
    m_ReceivedPackets.clear();
    m_PacketsToSend.clear();
    m_PendingAcks.clear();
    m_ProcessedSequences.clear();
}

bool ReliableChannel::Send(Packet packet) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_IsConnected) {
        return false;
    }
    if (m_SendWindow.size() >= m_Config.windowSize) {
        LOG_WARN("ReliableChannel", "Send window full (%zu/%u)",
            m_SendWindow.size(), m_Config.windowSize);
        return false;
    }
    uint32_t sequence = m_NextSendSequence.fetch_add(1, std::memory_order_relaxed);
    packet.SetSequence(sequence);
    
    auto currentTime = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    
    PendingPacket pending;
    pending.packet = std::move(packet);
    pending.sendTime = currentTime;
    pending.retransmitCount = 0;
    pending.acknowledged = false;
    m_SendWindow.push_back(std::move(pending));
    m_Statistics.packetsSent++;
    auto& lastPacket = m_SendWindow.back();
    m_PacketsToSend.push_back(lastPacket.packet);
    return true;
}

void ReliableChannel::Receive(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Packet packet;
    if (!Packet::Deserialize(data, size, packet)) {
        LOG_WARN("ReliableChannel", "Failed to deserialize packet");
        return;
    }
    m_Statistics.bytesReceived += size;
    if (packet.GetType() == PacketType::Ack) {
        uint32_t ackSequence;
        size_t offset = 0;
        if (packet.Read(ackSequence, offset)) {
            ProcessAck(ackSequence);
        }
        return;
    }
    if (packet.GetType() == PacketType::Nack) {
        uint32_t nackSequence;
        size_t offset = 0;
        if (packet.Read(nackSequence, offset)) {
            ProcessNack(nackSequence);
        }
        return;
    }
    ProcessPacket(packet);
}

void ReliableChannel::Update(uint32_t currentTimeMs) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_IsConnected) {
        return;
    }
    CheckRetransmissions(currentTimeMs);
    if (m_PacketsSinceLastAck >= m_Config.ackFrequency) {
        SendAck();
        m_PacketsSinceLastAck = 0;
    }
}

std::vector<Packet> ReliableChannel::GetReceivedPackets() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<Packet> packets;
    packets.swap(m_ReceivedPackets);
    return packets;
}

std::vector<Packet> ReliableChannel::GetPacketsToSend() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<Packet> packets;
    packets.swap(m_PacketsToSend);
    return packets;
}

ChannelStatistics ReliableChannel::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Statistics;
}

void ReliableChannel::ResetStatistics() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Statistics.Reset();
}

void ReliableChannel::ProcessPacket(const Packet& packet) {
    uint32_t sequence = packet.GetSequence();
    
    if (!IsSequenceValid(sequence)) {
        LOG_DEBUG("ReliableChannel", "Invalid sequence %u", sequence);
        return;
    }
    
    if (m_ProcessedSequences.find(sequence) != m_ProcessedSequences.end()) {
        LOG_DEBUG("ReliableChannel", "Duplicate sequence %u", sequence);
        m_Statistics.packetsAcknowledged++;
        SendAck();
        return;
    }
    
    if (m_ReassemblyBuffer.find(sequence) != m_ReassemblyBuffer.end()) {
        LOG_DEBUG("ReliableChannel", "Sequence %u already in buffer", sequence);
        return;
    }
    
    m_ReassemblyBuffer[sequence] = packet;
    m_ReceivedSequences.push_back(sequence);
    m_PacketsSinceLastAck++;
    
    uint32_t expected = m_NextExpectedSequence.load(std::memory_order_relaxed);
    while (m_ReassemblyBuffer.find(expected) != m_ReassemblyBuffer.end()) {
        m_ReceivedPackets.push_back(m_ReassemblyBuffer[expected]);
        m_ProcessedSequences.insert(expected);
        m_ReassemblyBuffer.erase(expected);
        
        if (m_ProcessedSequences.size() > MAX_PROCESSED_HISTORY) {
            uint32_t oldest = *m_ProcessedSequences.begin();
            m_ProcessedSequences.erase(oldest);
        }
        
        expected++;
    }
    m_NextExpectedSequence.store(expected, std::memory_order_relaxed);
    SendAck();
}

void ReliableChannel::ProcessAck(uint32_t ackSequence) {
    auto it = std::find_if(m_SendWindow.begin(), m_SendWindow.end(),
        [ackSequence](const PendingPacket& p) {
            return p.packet.GetSequence() == ackSequence;
        });
    if (it == m_SendWindow.end()) {
        return;
    }
    uint32_t rtt = 0;
    if (it->sendTime > 0) {
        auto currentTime = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        rtt = currentTime - it->sendTime;
    }
    m_SendWindow.erase(it);
    m_Statistics.packetsAcknowledged++;
    m_Statistics.currentRttMs = static_cast<float>(rtt);
    if (rtt > 0) {
        UpdateRtt(ackSequence, rtt);
    }
    InvokeOnPacketAcknowledged(ackSequence);
}

void ReliableChannel::ProcessNack(uint32_t nackSequence) {
    for (auto& pending : m_SendWindow) {
        if (pending.packet.GetSequence() == nackSequence) {
            pending.retransmitCount = m_Config.maxRetransmissions;
            break;
        }
    }
}

void ReliableChannel::SendAck() {
    Packet ackPacket(PacketType::Ack);
    uint32_t lastAck = m_NextExpectedSequence.load(std::memory_order_relaxed) - 1;
    ackPacket.Write(lastAck);
    m_PacketsToSend.push_back(std::move(ackPacket));
    m_LastAckSent = lastAck;
    m_PendingAcks.clear();
}

void ReliableChannel::SendNack(uint32_t sequence) {
    Packet nackPacket(PacketType::Nack);
    nackPacket.Write(sequence);
    m_PacketsToSend.push_back(std::move(nackPacket));
}

void ReliableChannel::CheckRetransmissions(uint32_t currentTimeMs) {
    for (auto& pending : m_SendWindow) {
        if (pending.acknowledged) {
            continue;
        }
        if (pending.sendTime == 0) {
            pending.sendTime = currentTimeMs;
            continue;
        }
        uint32_t timeout = static_cast<uint32_t>(
            m_SmoothedRtt * m_Config.rttTimeoutMultiplier);
        timeout = std::max(timeout, m_Config.rttTimeoutMs);
        if (currentTimeMs - pending.sendTime >= timeout) {
            if (pending.retransmitCount >= m_Config.maxRetransmissions) {
                m_Statistics.packetsLost++;
                InvokeOnPacketLost(pending.packet.GetSequence());
                pending.acknowledged = true;
            } else {
                RetransmitPacket(pending);
            }
        }
    }
}

void ReliableChannel::RetransmitPacket(PendingPacket& pending) {
    pending.retransmitCount++;
    pending.sendTime = 0;
    m_PacketsToSend.push_back(pending.packet);
    m_Statistics.packetsRetransmitted++;
    LOG_DEBUG("ReliableChannel", "Retransmitting packet %u (attempt %u/%u)",
        pending.packet.GetSequence(),
        pending.retransmitCount,
        m_Config.maxRetransmissions);
}

void ReliableChannel::UpdateRtt(uint32_t sequence, uint32_t rtt) {
    float rttFloat = static_cast<float>(rtt);
    m_RttSamples.push_back(rttFloat);
    if (m_RttSamples.size() > MAX_RTT_SAMPLES) {
        m_RttSamples.erase(m_RttSamples.begin());
    }
    if (m_SmoothedRtt == 0.0f) {
        m_SmoothedRtt = rttFloat;
        m_RttVariation = rttFloat * 0.5f;
    } else {
        float diff = std::abs(m_SmoothedRtt - rttFloat);
        m_SmoothedRtt = (1.0f - SMOOTHING_FACTOR) * m_SmoothedRtt +
            SMOOTHING_FACTOR * rttFloat;
        m_RttVariation = (1.0f - VARIATION_FACTOR) * m_RttVariation +
            VARIATION_FACTOR * diff;
    }
    m_Statistics.averageRttMs = m_SmoothedRtt;
    m_Statistics.currentRttMs = rttFloat;
    CalculateRttStatistics();
    InvokeOnRttUpdated(rttFloat);
}

void ReliableChannel::CalculateRttStatistics() {
    if (m_RttSamples.size() < 2) {
        m_Statistics.jitterMs = 0.0f;
        return;
    }
    float sum = 0.0f;
    for (size_t i = 1; i < m_RttSamples.size(); ++i) {
        sum += std::abs(m_RttSamples[i] - m_RttSamples[i - 1]);
    }
    m_Statistics.jitterMs = sum / static_cast<float>(m_RttSamples.size() - 1);
    if (m_Statistics.packetsSent > 0) {
        m_Statistics.packetLossRate =
            static_cast<float>(m_Statistics.packetsLost) /
            static_cast<float>(m_Statistics.packetsSent);
    }
}

bool ReliableChannel::IsSequenceValid(uint32_t sequence) const {
    uint32_t expected = m_NextExpectedSequence.load(std::memory_order_relaxed);
    int32_t diff = static_cast<int32_t>(sequence) - static_cast<int32_t>(expected);
    return diff >= -static_cast<int32_t>(m_Config.windowSize) &&
        diff <= static_cast<int32_t>(m_Config.windowSize);
}

bool ReliableChannel::IsSequenceInWindow(uint32_t sequence) const {
    if (m_SendWindow.empty()) {
        return false;
    }
    uint32_t oldest = m_SendWindow.front().packet.GetSequence();
    uint32_t newest = m_SendWindow.back().packet.GetSequence();
    return sequence >= oldest && sequence <= newest;
}

void ReliableChannel::InvokeOnPacketAcknowledged(uint32_t sequence) {
    if (m_OnPacketAcknowledged) {
        m_OnPacketAcknowledged(sequence);
    }
}

void ReliableChannel::InvokeOnPacketLost(uint32_t sequence) {
    if (m_OnPacketLost) {
        m_OnPacketLost(sequence);
    }
}

void ReliableChannel::InvokeOnRttUpdated(float rttMs) {
    if (m_OnRttUpdated) {
        m_OnRttUpdated(rttMs);
    }
}

} // namespace SagaEngine::Networking