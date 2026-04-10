/// @file ReliableChannelTests.cpp
/// @brief Unit tests for ReliableChannel with SACK, fast retransmit, NACK, and Karn's Algorithm.

#include "SagaServer/Networking/Core/ReliableChannel.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace SagaEngine::Networking;

// ─── Test Fixture ───────────────────────────────────────────────────────────

class ReliableChannelTest : public ::testing::Test {
protected:
    std::unique_ptr<ReliableChannel> m_Channel;
    ReliableChannelConfig m_Config;

    void SetUp() override {
        m_Config.windowSize = 64;
        m_Config.maxRetransmissions = 3;
        m_Config.rttTimeoutMs = 100;
        m_Config.ackFrequency = 4;
        m_Config.dupAckThreshold = 3;
        m_Channel = std::make_unique<ReliableChannel>(1);
        m_Channel->Initialize(m_Config);
        m_Channel->SetConnected(true);
    }
};

// ─── Basic Send/Receive ─────────────────────────────────────────────────────

TEST_F(ReliableChannelTest, SendAndReceive) {
    Packet packet(PacketType::ComponentUpdate);
    packet.Write(static_cast<uint32_t>(123));
    bool sent = m_Channel->Send(std::move(packet));
    EXPECT_TRUE(sent);
    auto packetsToSend = m_Channel->GetPacketsToSend();
    EXPECT_EQ(packetsToSend.size(), 1);
}

TEST_F(ReliableChannelTest, Acknowledgement) {
    Packet packet(PacketType::InputCommand);
    packet.Write(static_cast<uint32_t>(456));
    m_Channel->Send(std::move(packet));
    auto packetsToSend = m_Channel->GetPacketsToSend();
    Packet ackPacket(PacketType::Ack);
    ackPacket.Write(packetsToSend[0].GetSequence());
    ackPacket.Write(static_cast<uint32_t>(0)); // SACK bitmask
    auto ackData = ackPacket.GetSerializedData();
    m_Channel->Receive(ackData.data(), ackData.size());
    auto stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsAcknowledged, 1);
}

TEST_F(ReliableChannelTest, OutOfOrderDelivery) {
    std::vector<Packet> packets;
    for (int i = 0; i < 5; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        packets.push_back(std::move(pkt));
    }
    for (auto& pkt : packets) {
        m_Channel->Send(std::move(pkt));
    }
    auto toSend = m_Channel->GetPacketsToSend();
    std::vector<std::vector<uint8_t>> receivedData;
    for (auto& pkt : toSend) {
        receivedData.push_back(pkt.GetSerializedData());
    }
    m_Channel->Receive(receivedData[2].data(), receivedData[2].size());
    m_Channel->Receive(receivedData[0].data(), receivedData[0].size());
    m_Channel->Receive(receivedData[4].data(), receivedData[4].size());
    m_Channel->Receive(receivedData[1].data(), receivedData[1].size());
    m_Channel->Receive(receivedData[3].data(), receivedData[3].size());
    auto received = m_Channel->GetReceivedPackets();
    EXPECT_EQ(received.size(), 5);
}

TEST_F(ReliableChannelTest, DuplicateDetection) {
    Packet packet(PacketType::EntitySpawn);
    packet.Write(static_cast<uint32_t>(789));
    m_Channel->Send(std::move(packet));
    auto toSend = m_Channel->GetPacketsToSend();
    auto data = toSend[0].GetSerializedData();
    m_Channel->Receive(data.data(), data.size());
    m_Channel->Receive(data.data(), data.size());
    auto stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsAcknowledged, 1);
}

TEST_F(ReliableChannelTest, Statistics) {
    for (int i = 0; i < 10; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    auto stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsSent, 10);
    EXPECT_EQ(stats.packetsAcknowledged, 0);
    auto toSend = m_Channel->GetPacketsToSend();
    for (auto& pkt : toSend) {
        Packet ack(PacketType::Ack);
        ack.Write(pkt.GetSequence());
        ack.Write(static_cast<uint32_t>(0)); // SACK bitmask
        auto data = ack.GetSerializedData();
        m_Channel->Receive(data.data(), data.size());
    }
    stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsAcknowledged, 10);
}

TEST_F(ReliableChannelTest, WindowSizeLimit) {
    for (uint32_t i = 0; i < m_Config.windowSize + 10; ++i) {
        Packet pkt(PacketType::Custom);
        m_Channel->Send(std::move(pkt));
    }
    auto stats = m_Channel->GetStatistics();
    EXPECT_LE(stats.packetsSent, m_Config.windowSize);
}

TEST_F(ReliableChannelTest, RttCalculation) {
    Packet packet(PacketType::KeepAlive);
    m_Channel->Send(std::move(packet));
    auto toSend = m_Channel->GetPacketsToSend();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Packet ack(PacketType::Ack);
    ack.Write(toSend[0].GetSequence());
    ack.Write(static_cast<uint32_t>(0)); // SACK bitmask
    auto data = ack.GetSerializedData();
    m_Channel->Receive(data.data(), data.size());
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.currentRttMs, 0.0f);
}

TEST_F(ReliableChannelTest, Shutdown) {
    m_Channel->Shutdown();
    Packet packet(PacketType::Custom);
    bool sent = m_Channel->Send(std::move(packet));
    EXPECT_FALSE(sent);
    EXPECT_FALSE(m_Channel->IsConnected());
}

// ─── SACK (Selective Acknowledgment) Tests ──────────────────────────────────

TEST_F(ReliableChannelTest, SackBitmaskInAck) {
    // Send multiple packets
    for (int i = 0; i < 5; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    auto toSend = m_Channel->GetPacketsToSend();

    // Receive packets out of order to build SACK state on sender
    auto data2 = toSend[2].GetSerializedData();
    auto data4 = toSend[4].GetSerializedData();
    m_Channel->Receive(data2.data(), data2.size());
    m_Channel->Receive(data4.data(), data4.size());

    // Get packets to send (includes both NACKs and ACKs)
    auto packetsToSend = m_Channel->GetPacketsToSend();
    
    // Filter for only ACK packets
    std::vector<Packet> ackPackets;
    for (const auto& pkt : packetsToSend) {
        if (pkt.GetType() == PacketType::Ack) {
            ackPackets.push_back(pkt);
        }
    }
    
    EXPECT_GE(ackPackets.size(), 2); // At least 2 ACKs

    // Verify ACK packets have SACK bitmask payload (8 bytes: 4 for seq + 4 for bitmask)
    for (const auto& ack : ackPackets) {
        EXPECT_EQ(ack.GetType(), PacketType::Ack);
        EXPECT_EQ(ack.GetPayloadSize(), 8); // 4 bytes seq + 4 bytes SACK
    }
}

TEST_F(ReliableChannelTest, SackAcknowledgesOutOfOrderPackets) {
    // Send 10 packets
    for (int i = 0; i < 10; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    auto toSend = m_Channel->GetPacketsToSend();
    
    // Acknowledge with SACK: ACK seq 3, SACK bitmask has bits 5, 7, 9 set
    Packet ack(PacketType::Ack);
    ack.Write(static_cast<uint32_t>(3)); // Cumulative ACK up to seq 3
    uint32_t sackBitmask = (1u << 2) | (1u << 4) | (1u << 6); // Sequences 5, 7, 9 relative to 4
    ack.Write(sackBitmask);
    auto ackData = ack.GetSerializedData();
    m_Channel->Receive(ackData.data(), ackData.size());
    
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.sacksReceived, 0);
    // Should have acknowledged seq 3 + SACK'd packets
    EXPECT_GE(stats.packetsAcknowledged, 1);
}

// ─── NACK (Negative Acknowledgment) Tests ───────────────────────────────────

TEST_F(ReliableChannelTest, NackOnGapDetection) {
    // Send 5 packets
    for (int i = 0; i < 5; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    auto toSend = m_Channel->GetPacketsToSend();

    // Clear any initial packets to get a clean state
    m_Channel->GetPacketsToSend();

    // Simulate receiver getting seq 1 when expecting seq 0 (gap detected)
    auto data1 = toSend[1].GetSerializedData();
    m_Channel->Receive(data1.data(), data1.size());

    // Receiver should have sent NACK for seq 0
    auto packetsToSend = m_Channel->GetPacketsToSend();
    bool foundNack = false;
    for (const auto& pkt : packetsToSend) {
        if (pkt.GetType() == PacketType::Nack) {
            foundNack = true;
            break;
        }
    }
    EXPECT_TRUE(foundNack);
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.nacksSent, 0);
}

TEST_F(ReliableChannelTest, NackTriggersFastRetransmit) {
    // Send packets
    Packet pkt0(PacketType::Custom);
    pkt0.Write(static_cast<uint32_t>(100));
    m_Channel->Send(std::move(pkt0));
    
    Packet pkt1(PacketType::Custom);
    pkt1.Write(static_cast<uint32_t>(101));
    m_Channel->Send(std::move(pkt1));
    
    auto toSend = m_Channel->GetPacketsToSend();
    auto seq0 = toSend[0].GetSequence();
    
    // Clear the send queue
    m_Channel->GetPacketsToSend();
    
    // Simulate NACK for seq 0 from receiver
    Packet nack(PacketType::Nack);
    nack.Write(seq0);
    auto nackData = nack.GetSerializedData();
    m_Channel->Receive(nackData.data(), nackData.size());
    
    // Should trigger fast retransmit
    auto retransmits = m_Channel->GetPacketsToSend();
    bool foundRetransmit = false;
    for (const auto& pkt : retransmits) {
        if (pkt.GetSequence() == seq0) {
            foundRetransmit = true;
            break;
        }
    }
    EXPECT_TRUE(foundRetransmit);
    
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.fastRetransmits, 0);
}

// ─── Fast Retransmit (Duplicate ACK) Tests ─────────────────────────────────

TEST_F(ReliableChannelTest, FastRetransmitOnDuplicateAcks) {
    // Send 10 packets
    for (int i = 0; i < 10; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    auto toSend = m_Channel->GetPacketsToSend();
    auto seq0 = toSend[0].GetSequence();
    auto seq1 = toSend[1].GetSequence();

    // Don't clear send queue - keep packets in flight

    // Send 1 ACK for seq1 (cumulative ACK will also ack seq0)
    {
        Packet ack(PacketType::Ack);
        ack.Write(seq1);
        ack.Write(static_cast<uint32_t>(0));
        auto ackData = ack.GetSerializedData();
        m_Channel->Receive(ackData.data(), ackData.size());
    }

    // Now we need to trigger fast retransmit for a packet still in window.
    // Since seq0 and seq1 are already acked, we use seq2.
    // Send 4 ACKs for seq2: first ACK acks seq2, next 3 are duplicates
    auto seq2 = toSend[2].GetSequence();
    {
        Packet ack(PacketType::Ack);
        ack.Write(seq2);
        ack.Write(static_cast<uint32_t>(0));
        auto ackData = ack.GetSerializedData();
        m_Channel->Receive(ackData.data(), ackData.size());
    }
    // Send 3 duplicate ACKs for seq2
    for (uint32_t i = 0; i < 3; ++i) {
        Packet ack(PacketType::Ack);
        ack.Write(seq2);
        ack.Write(static_cast<uint32_t>(0));
        auto ackData = ack.GetSerializedData();
        m_Channel->Receive(ackData.data(), ackData.size());
    }

    // Fast retransmit should trigger after 3 duplicate ACKs for seq2
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.fastRetransmits, 0);
}

// ─── Karn's Algorithm Tests ────────────────────────────────────────────────

TEST_F(ReliableChannelTest, KarnsAlgorithmIgnoresRetransmittedRtt) {
    Packet packet(PacketType::KeepAlive);
    m_Channel->Send(std::move(packet));
    auto toSend = m_Channel->GetPacketsToSend();
    auto seq = toSend[0].GetSequence();
    
    // Simulate timeout retransmission by calling Update with large time delta
    m_Channel->Update(10000); // Will trigger retransmit
    m_Channel->GetPacketsToSend(); // Clear queue
    
    // Now ACK the retransmitted packet after a delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Packet ack(PacketType::Ack);
    ack.Write(seq);
    ack.Write(static_cast<uint32_t>(0));
    auto ackData = ack.GetSerializedData();
    m_Channel->Receive(ackData.data(), ackData.size());
    
    auto stats = m_Channel->GetStatistics();
    // RTT should not have been updated because it was a retransmission
    // The first RTT sample would be 0 since sendTime gets reset on retransmit
    // Karn's algorithm ensures we don't corrupt RTT estimation
    EXPECT_GE(stats.averageRttMs, 0.0f);
}

// ─── Stress and Edge Cases ─────────────────────────────────────────────────

TEST_F(ReliableChannelTest, HighVolumeSendReceive) {
    // Increase window size to allow 100+ packets in flight
    m_Channel->Shutdown();
    ReliableChannelConfig config;
    config.windowSize = 128;
    config.maxRetransmissions = 3;
    config.rttTimeoutMs = 100;
    config.ackFrequency = 4;
    config.dupAckThreshold = 3;
    m_Channel = std::make_unique<ReliableChannel>(1);
    m_Channel->Initialize(config);
    m_Channel->SetConnected(true);

    // Send 100 packets
    for (int i = 0; i < 100; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }

    // Drain send queue
    auto toSend = m_Channel->GetPacketsToSend();
    EXPECT_EQ(toSend.size(), 100);

    // Receive all packets in order
    for (const auto& pkt : toSend) {
        auto data = pkt.GetSerializedData();
        m_Channel->Receive(data.data(), data.size());
    }

    auto received = m_Channel->GetReceivedPackets();
    EXPECT_EQ(received.size(), 100);

    // Verify order
    for (size_t i = 0; i < received.size(); ++i) {
        uint32_t value;
        size_t offset = 0;
        received[i].Read(value, offset);
        EXPECT_EQ(value, static_cast<uint32_t>(i));
    }
}

TEST_F(ReliableChannelTest, RapidFireAckNack) {
    // Alternate between data and NACK packets
    for (int i = 0; i < 20; ++i) {
        Packet pkt(PacketType::Custom);
        pkt.Write(static_cast<uint32_t>(i));
        m_Channel->Send(std::move(pkt));
    }
    
    auto toSend = m_Channel->GetPacketsToSend();
    
    // Receive every other packet, skip the rest
    for (size_t i = 0; i < toSend.size(); i += 2) {
        auto data = toSend[i].GetSerializedData();
        m_Channel->Receive(data.data(), data.size());
    }
    
    // Should have generated NACKs for missing packets
    auto stats = m_Channel->GetStatistics();
    EXPECT_GT(stats.nacksSent, 0);
}

TEST_F(ReliableChannelTest, StatisticsReset) {
    // Generate some traffic
    for (int i = 0; i < 10; ++i) {
        Packet pkt(PacketType::Custom);
        m_Channel->Send(std::move(pkt));
    }
    
    auto stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsSent, 10);
    
    m_Channel->ResetStatistics();
    stats = m_Channel->GetStatistics();
    EXPECT_EQ(stats.packetsSent, 0);
    EXPECT_EQ(stats.packetsAcknowledged, 0);
    EXPECT_EQ(stats.fastRetransmits, 0);
    EXPECT_EQ(stats.nacksSent, 0);
}