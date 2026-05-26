/// @file ZoneServerPacketDrainTests.cpp
/// @brief Tests for ZoneServer raw inbound frame normalization.

#include "SagaServer/Networking/Server/ZoneServer.h"

#include <gtest/gtest.h>

#include <cstring>
#include <cstdint>
#include <vector>

namespace
{

using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaServer::Networking::IZoneServerListener;
using SagaServer::Networking::NormalizedServerPacketView;
using SagaServer::Networking::ServerPacketNormalizationStatus;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

std::vector<std::uint8_t> MakeFrame(PacketType packetType,
                                    const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> frame(
        SagaServer::Networking::kServerPacketFrameHeaderSize + payload.size());

    const std::uint16_t magic =
        SagaServer::Networking::kServerPacketFrameMagic;
    const std::uint32_t bodyLength =
        static_cast<std::uint32_t>(payload.size());

    std::memcpy(frame.data(), &magic, sizeof(magic));
    frame[2] = static_cast<std::uint8_t>(packetType);
    frame[3] = 0;
    std::memcpy(frame.data() + 4, &bodyLength, sizeof(bodyLength));
    if (!payload.empty())
    {
        std::memcpy(
            frame.data() + SagaServer::Networking::kServerPacketFrameHeaderSize,
            payload.data(),
            payload.size());
    }
    return frame;
}

ZoneServerConfig TestConfig()
{
    ZoneServerConfig config;
    config.bindAddress = "127.0.0.1";
    config.port = 0;
    config.zoneId = 11;
    config.zoneName = "packet-drain-test";
    config.enableDetailedPacketLog = false;
    return config;
}

struct PacketObserver final : IZoneServerListener
{
    void OnInboundPacketNormalized(
        const NormalizedServerPacketView& packet) override
    {
        ++callCount;
        clientId = packet.clientId;
        packetType = packet.packetType;
        payload = packet.payload;
        payloadSize = packet.payloadSize;
        payloadBytes.clear();
        if (packet.payload != nullptr && packet.payloadSize > 0)
        {
            payloadBytes.assign(
                packet.payload, packet.payload + packet.payloadSize);
        }
    }

    int callCount{0};
    ClientId clientId{0};
    PacketType packetType{PacketType::Invalid};
    const std::uint8_t* payload{nullptr};
    std::size_t payloadSize{0};
    std::vector<std::uint8_t> payloadBytes;
};

} // namespace

TEST(ZoneServerPacketDrainTests, ValidInputFrameReachesNormalizedHandler)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);

    const ClientId clientId = 42;
    const std::vector<std::uint8_t> payload{0x10, 0x20, 0x30};
    const auto frame = MakeFrame(PacketType::InputCommand, payload);

    const auto status = server.ProcessRawInboundPacketForTesting(
        clientId, frame.data(), frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::Success);
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_EQ(observer.clientId, clientId);
    EXPECT_EQ(observer.packetType, PacketType::InputCommand);
    EXPECT_EQ(observer.payload, frame.data() +
                                SagaServer::Networking::kServerPacketFrameHeaderSize);
    EXPECT_EQ(observer.payloadSize, payload.size());
    EXPECT_EQ(observer.payloadBytes, payload);
    EXPECT_EQ(server.GetStats().totalPacketsNormalized.load(), 1u);
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 0u);
}

TEST(ZoneServerPacketDrainTests, ValidNonInputFrameStillNormalizes)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);

    const auto frame = MakeFrame(PacketType::KeepAlive, {0x01, 0x02});

    const auto status = server.ProcessRawInboundPacketForTesting(
        7, frame.data(), frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::Success);
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_EQ(observer.clientId, 7u);
    EXPECT_EQ(observer.packetType, PacketType::KeepAlive);
    EXPECT_EQ(observer.payloadSize, 2u);
    EXPECT_EQ(observer.payloadBytes, (std::vector<std::uint8_t>{0x01, 0x02}));
    EXPECT_EQ(server.GetStats().totalPacketsNormalized.load(), 1u);
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 0u);
}

TEST(ZoneServerPacketDrainTests, MalformedFrameRejectsBeforeHandler)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);

    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[0] ^= 0xFF;

    const auto status = server.ProcessRawInboundPacketForTesting(
        1, frame.data(), frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::BadMagic);
    EXPECT_EQ(observer.callCount, 0);
    EXPECT_EQ(server.GetStats().totalPacketsNormalized.load(), 0u);
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 1u);
}

TEST(ZoneServerPacketDrainTests, BodyLengthMismatchRejectsBeforeHandler)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);

    auto frame = MakeFrame(PacketType::InputCommand, {0x01, 0x02});
    const std::uint32_t wrongBodyLength = 7;
    std::memcpy(frame.data() + 4, &wrongBodyLength, sizeof(wrongBodyLength));

    const auto status = server.ProcessRawInboundPacketForTesting(
        1, frame.data(), frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::BodyLengthMismatch);
    EXPECT_EQ(observer.callCount, 0);
    EXPECT_EQ(server.GetStats().totalPacketsNormalized.load(), 0u);
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 1u);
}

TEST(ZoneServerPacketDrainTests, InvalidPacketTypeRejectsBeforeHandler)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);

    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[2] = 99;

    const auto status = server.ProcessRawInboundPacketForTesting(
        1, frame.data(), frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::InvalidPacketType);
    EXPECT_EQ(observer.callCount, 0);
    EXPECT_EQ(server.GetStats().totalPacketsNormalized.load(), 0u);
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 1u);
}
