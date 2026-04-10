/// @file ReliableChannel.cpp
/// @brief Selective-repeat ARQ implementation with SACK, fast retransmit, and Karn's Algorithm.

#include "SagaServer/Networking/Core/ReliableChannel.h"
#include "SagaEngine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>

// ─── Constants ──────────────────────────────────────────────────────────────

namespace SagaEngine::Networking {

constexpr float SMOOTHING_FACTOR = 0.125f;   ///< EWMA alpha for RTT smoothing
constexpr float VARIATION_FACTOR = 0.25f;    ///< EWMA alpha for RTT variance
constexpr size_t MAX_RTT_SAMPLES = 100;      ///< Cap on RTT sample history
constexpr size_t MAX_PROCESSED_HISTORY = 1024; ///< Max processed sequences to retain

// ─── Construction & Lifecycle ───────────────────────────────────────────────

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
    m_DupAckCounts.clear();
    m_NextSendSequence.store(1, std::memory_order_relaxed);
    m_NextExpectedSequence.store(1, std::memory_order_relaxed);
    m_SackBitmask = 0;
    m_SmoothedRtt = 0.0f;
    m_RttVariation = 0.0f;
    m_LastRttUpdateTime = 0;
    m_LastAckSent = 0;
    m_PacketsSinceLastAck = 0;
    m_Statistics.Reset();
    LOG_INFO("ReliableChannel", "Initialized for connection %llu (window=%u, maxRetrans=%u)",
        m_ConnectionId, config.windowSize, config.maxRetransmissions);
    return true;
}

void ReliableChannel::Shutdown() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    LOG_INFO("ReliableChannel", "Shutting down (sent=%llu, acked=%llu, lost=%llu, fastRetx=%llu)",
        m_Statistics.packetsSent,
        m_Statistics.packetsAcknowledged,
        m_Statistics.packetsLost,
        m_Statistics.fastRetransmits);
    m_IsConnected = false;
    m_SendWindow.clear();
    m_ReassemblyBuffer.clear();
    m_ReceivedPackets.clear();
    m_PacketsToSend.clear();
    m_PendingAcks.clear();
    m_ProcessedSequences.clear();
    m_DupAckCounts.clear();
}

// ─── Send Path ──────────────────────────────────────────────────────────────

bool ReliableChannel::Send(Packet packet) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_IsConnected) {
        return false;
    }
    
    // Allow unlimited queuing, but only track window_size as "in flight"
    // The window size limit applies to unacknowledged packets waiting for ACK
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
    pending.isRetransmission = false;
    m_SendWindow.push_back(std::move(pending));
    m_Statistics.packetsSent++;
    auto& lastPacket = m_SendWindow.back();
    m_PacketsToSend.push_back(lastPacket.packet);
    return true;
}

// ─── Receive Path ───────────────────────────────────────────────────────────

void ReliableChannel::Receive(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Packet packet;
    if (!Packet::Deserialize(data, size, packet)) {
        LOG_WARN("ReliableChannel", "Failed to deserialize packet");
        return;
    }
    m_Statistics.bytesReceived += size;
    if (packet.GetType() == PacketType::Ack) {
        // ACK payload: cumulative ACK sequence (4 bytes) + SACK bitmask (4 bytes)
        uint32_t ackSequence = 0;
        uint32_t sackBitmask = 0;
        size_t offset = 0;
        if (packet.Read(ackSequence, offset)) {
            packet.Read(sackBitmask, offset); // SACK is optional
            ProcessAck(ackSequence, sackBitmask);
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

// ─── Inbound Packet Processing ──────────────────────────────────────────────

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

    LOG_INFO("ReliableChannel", "Processing seq %u (expected %u)", sequence, 
             m_NextExpectedSequence.load(std::memory_order_relaxed));

    m_ReassemblyBuffer[sequence] = packet;
    m_ReceivedSequences.push_back(sequence);
    m_PacketsSinceLastAck++;

    // NACK on gap detection: if this packet is beyond expected and there's a hole
    uint32_t expected = m_NextExpectedSequence.load(std::memory_order_relaxed);
    if (sequence > expected) {
        LOG_INFO("ReliableChannel", "Gap detected: expected %u, got %u. Sending NACK.", 
                 expected, sequence);
        // Send NACK for the expected sequence to trigger fast retransmit on sender
        SendNack(expected);
        m_Statistics.nacksSent++;
    }

    // Deliver in-order contiguous block
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

// ─── ACK Processing with SACK ───────────────────────────────────────────────

void ReliableChannel::ProcessAck(uint32_t ackSequence, uint32_t sackBitmask) {
    LOG_INFO("ReliableChannel", "ProcessAck: ackSeq=%u, sackMask=0x%X, windowSize=%zu", 
             ackSequence, sackBitmask, m_SendWindow.size());

    // Process cumulative ACK
    auto it = std::find_if(m_SendWindow.begin(), m_SendWindow.end(),
        [ackSequence](const PendingPacket& p) {
            return p.packet.GetSequence() == ackSequence;
        });
    if (it == m_SendWindow.end()) {
        // Packet already acknowledged or not found - track duplicate ACK count
        // This is used for fast retransmit detection
        m_DupAckCounts[ackSequence]++;
        LOG_INFO("ReliableChannel", "Duplicate ACK for seq %u (count=%u, threshold=%u)", 
                 ackSequence, m_DupAckCounts[ackSequence], m_Config.dupAckThreshold);

        // Check if we should trigger fast retransmit
        if (m_DupAckCounts[ackSequence] >= m_Config.dupAckThreshold) {
            LOG_INFO("ReliableChannel", "Fast retransmit triggered for seq %u (dup ACKs=%u)", 
                     ackSequence, m_DupAckCounts[ackSequence]);
            FastRetransmitPacket(ackSequence);
        }
        return;
    }

    LOG_INFO("ReliableChannel", "ACK received for seq %u, removing from window", ackSequence);

    uint32_t rtt = 0;
    if (it->sendTime > 0) {
        auto currentTime = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        rtt = currentTime - it->sendTime;
    }

    bool isRetransmission = it->isRetransmission;
    m_SendWindow.erase(it);
    m_Statistics.packetsAcknowledged++;
    m_Statistics.currentRttMs = static_cast<float>(rtt);

    // Karn's Algorithm: don't update RTT from retransmitted packets
    if (rtt > 0 && !isRetransmission) {
        UpdateRtt(ackSequence, rtt, false);
    }

    // Process SACK bitmask: acknowledge packets indicated by sender as received
    if (sackBitmask != 0) {
        m_Statistics.sacksReceived++;
        uint32_t baseSeq = ackSequence + 1;
        for (uint32_t bit = 0; bit < kSackBitmaskSize; ++bit) {
            if (sackBitmask & (1u << bit)) {
                uint32_t sackSeq = baseSeq + bit;
                auto sackIt = std::find_if(m_SendWindow.begin(), m_SendWindow.end(),
                    [sackSeq](const PendingPacket& p) {
                        return p.packet.GetSequence() == sackSeq;
                    });
                if (sackIt != m_SendWindow.end()) {
                    // SACK'd packet: count as duplicate ACK for fast retransmit tracking
                    m_DupAckCounts[sackSeq]++;
                    m_SendWindow.erase(sackIt);
                    m_Statistics.packetsAcknowledged++;
                }
            }
        }
    }

    // Reset dup ACK count for the cumulatively ACK'd packet
    m_DupAckCounts.erase(ackSequence);

    InvokeOnPacketAcknowledged(ackSequence);
}

void ReliableChannel::ProcessNack(uint32_t nackSequence) {
    // NACK means the receiver detected a gap; fast retransmit the missing packet
    FastRetransmitPacket(nackSequence);
}

// ─── ACK / NACK Transmission ────────────────────────────────────────────────

void ReliableChannel::SendAck() {
    Packet ackPacket(PacketType::Ack);
    uint32_t lastAck = m_NextExpectedSequence.load(std::memory_order_relaxed) - 1;
    uint32_t sackBitmask = BuildSackBitmask();
    ackPacket.Write(lastAck);
    ackPacket.Write(sackBitmask);
    m_PacketsToSend.push_back(std::move(ackPacket));
    m_LastAckSent = lastAck;
    m_PendingAcks.clear();
}

void ReliableChannel::SendNack(uint32_t sequence) {
    Packet nackPacket(PacketType::Nack);
    nackPacket.Write(sequence);
    m_PacketsToSend.push_back(std::move(nackPacket));
}

// ─── Retransmission Logic ───────────────────────────────────────────────────

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
                m_Statistics.timeoutRetransmits++;
                RetransmitPacket(pending);
            }
        }
    }
}

void ReliableChannel::FastRetransmitPacket(uint32_t duplicateAckSeq) {
    LOG_INFO("ReliableChannel", "FastRetransmitPacket: dupAck for seq %u, windowSize=%zu", 
             duplicateAckSeq, m_SendWindow.size());

    // Find the first unacknowledged packet AFTER the duplicate ACK sequence.
    // With cumulative ACKs, receiving duplicate ACKs for seq X means the receiver
    // has already received X but is waiting for packets after X.
    // We should retransmit the earliest unacked packet in the window.
    for (auto& pending : m_SendWindow) {
        if (!pending.acknowledged) {
            LOG_INFO("ReliableChannel", "Fast retransmitting seq %u (earliest unacked, dupAck for %u)", 
                     pending.packet.GetSequence(), duplicateAckSeq);
            pending.retransmitCount++;
            pending.sendTime = 0; // Trigger immediate resend on next Update
            pending.isRetransmission = true;
            m_PacketsToSend.push_back(pending.packet);
            m_Statistics.packetsRetransmitted++;
            m_Statistics.fastRetransmits++;
            m_DupAckCounts.erase(duplicateAckSeq);
            return;
        }
    }

    LOG_DEBUG("ReliableChannel", "FastRetransmitPacket: no unacked packets found for seq %u", duplicateAckSeq);
}

void ReliableChannel::RetransmitPacket(PendingPacket& pending) {
    pending.retransmitCount++;
    pending.sendTime = 0; // Will be set to current time on next transmission
    pending.isRetransmission = true;
    m_PacketsToSend.push_back(pending.packet);
    m_Statistics.packetsRetransmitted++;
    LOG_DEBUG("ReliableChannel", "Timeout retransmit for packet %u (attempt %u/%u)",
        pending.packet.GetSequence(),
        pending.retransmitCount,
        m_Config.maxRetransmissions);
}

// ─── RTT Estimation (Karn's Algorithm) ──────────────────────────────────────

void ReliableChannel::UpdateRtt(uint32_t /*sequence*/, uint32_t rtt, bool isRetransmission) {
    // Karn's Algorithm: skip RTT update if this is a retransmission
    // because we can't tell if this ACK is for the original or the retransmit
    if (isRetransmission) {
        return;
    }

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

// ─── Sequence Validation ────────────────────────────────────────────────────

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

// ─── SACK Bitmask ───────────────────────────────────────────────────────────

uint32_t ReliableChannel::BuildSackBitmask() const {
    // Build a 32-bit bitmask of sequences received after m_NextExpectedSequence
    // Bit 0 = expected, Bit 1 = expected+1, etc.
    uint32_t bitmask = 0;
    uint32_t expected = m_NextExpectedSequence.load(std::memory_order_relaxed);
    for (uint32_t seq : m_ReceivedSequences) {
        if (seq >= expected && seq < expected + kSackBitmaskSize) {
            uint32_t bit = seq - expected;
            bitmask |= (1u << bit);
        }
    }
    return bitmask;
}

bool ReliableChannel::IsSetInSack(uint32_t sequence) const {
    uint32_t expected = m_NextExpectedSequence.load(std::memory_order_relaxed);
    if (sequence < expected || sequence >= expected + kSackBitmaskSize) {
        return false;
    }
    uint32_t bit = sequence - expected;
    return (m_SackBitmask & (1u << bit)) != 0;
}

// ─── Callback Invokers ──────────────────────────────────────────────────────

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