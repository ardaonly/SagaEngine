#include "SagaEngine/Networking/Core/ReliableChannel.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace SagaEngine::Networking;

class ReliableChannelTest : public ::testing::Test {
protected:
    std::unique_ptr<ReliableChannel> m_Channel;
    ReliableChannelConfig m_Config;

    void SetUp() override {
        m_Config.windowSize = 64;
        m_Config.maxRetransmissions = 3;
        m_Config.rttTimeoutMs = 100;
        m_Config.ackFrequency = 4;
        m_Channel = std::make_unique<ReliableChannel>(1);
        m_Channel->Initialize(m_Config);
        m_Channel->SetConnected(true);
    }
};

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