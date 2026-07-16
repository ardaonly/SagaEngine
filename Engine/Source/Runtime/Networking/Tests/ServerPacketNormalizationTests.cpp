/// @file ServerPacketNormalizationTests.cpp
/// @brief Tests for ServerConnection frame normalization.

#include "SagaEngine/Networking/ServerPacketNormalizer.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace
{

using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaEngine::Networking::NormalizeServerPacketFrame;
using SagaEngine::Networking::ServerPacketNormalizationStatus;

std::vector<std::uint8_t> MakeFrame(PacketType packetType,
                                    const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> frame(
        SagaEngine::Networking::kServerPacketFrameHeaderSize + payload.size());

    const std::uint16_t magic =
        SagaEngine::Networking::kServerPacketFrameMagic;
    const std::uint32_t bodyLength =
        static_cast<std::uint32_t>(payload.size());

    std::memcpy(frame.data(), &magic, sizeof(magic));
    frame[2] = static_cast<std::uint8_t>(packetType);
    frame[3] = 0;
    std::memcpy(frame.data() + 4, &bodyLength, sizeof(bodyLength));
    if (!payload.empty())
    {
        std::memcpy(
            frame.data() + SagaEngine::Networking::kServerPacketFrameHeaderSize,
            payload.data(),
            payload.size());
    }
    return frame;
}

} // namespace

TEST(ServerPacketNormalizationTests, ValidInputCommandFrameNormalizesPayload)
{
    const ClientId clientId = 42;
    const std::vector<std::uint8_t> payload{0x10, 0x20, 0x30, 0x40};
    const auto frame = MakeFrame(PacketType::InputCommand, payload);

    const auto result =
        NormalizeServerPacketFrame(clientId, frame.data(), frame.size());

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::Success);
    EXPECT_EQ(result.packet.clientId, clientId);
    EXPECT_EQ(result.packet.packetType, PacketType::InputCommand);
    ASSERT_EQ(result.packet.payloadSize, payload.size());
    EXPECT_EQ(result.packet.payload, frame.data() + 8);
    EXPECT_EQ(std::memcmp(result.packet.payload, payload.data(), payload.size()), 0);
}

TEST(ServerPacketNormalizationTests, ValidNonInputFrameStillNormalizes)
{
    const auto frame = MakeFrame(PacketType::KeepAlive, {0x01, 0x02});

    const auto result = NormalizeServerPacketFrame(7, frame.data(), frame.size());

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.packet.clientId, 7u);
    EXPECT_EQ(result.packet.packetType, PacketType::KeepAlive);
    EXPECT_EQ(result.packet.payloadSize, 2u);
}

TEST(ServerPacketNormalizationTests, ShortFrameRejects)
{
    const std::vector<std::uint8_t> frame(7, 0);

    const auto result = NormalizeServerPacketFrame(1, frame.data(), frame.size());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::ShortFrame);
}

TEST(ServerPacketNormalizationTests, NullDataRejects)
{
    const auto result = NormalizeServerPacketFrame(1, nullptr, 8);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::NullData);
}

TEST(ServerPacketNormalizationTests, BadMagicRejects)
{
    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[0] ^= 0xFF;

    const auto result = NormalizeServerPacketFrame(1, frame.data(), frame.size());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::BadMagic);
}

TEST(ServerPacketNormalizationTests, DeclaredBodyLengthMismatchRejects)
{
    auto frame = MakeFrame(PacketType::InputCommand, {0x01, 0x02});
    const std::uint32_t wrongBodyLength = 7;
    std::memcpy(frame.data() + 4, &wrongBodyLength, sizeof(wrongBodyLength));

    const auto result = NormalizeServerPacketFrame(1, frame.data(), frame.size());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status,
              ServerPacketNormalizationStatus::BodyLengthMismatch);
}

TEST(ServerPacketNormalizationTests, OversizedBodyLengthRejects)
{
    auto frame = MakeFrame(PacketType::InputCommand, {});
    const std::uint32_t oversized =
        SagaEngine::Networking::kServerPacketFrameMaxBodyBytes + 1;
    std::memcpy(frame.data() + 4, &oversized, sizeof(oversized));

    const auto result = NormalizeServerPacketFrame(1, frame.data(), frame.size());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::BodyTooLarge);
}

TEST(ServerPacketNormalizationTests, InvalidPacketTypeRejects)
{
    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[2] = 99;

    const auto result = NormalizeServerPacketFrame(1, frame.data(), frame.size());

    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.status, ServerPacketNormalizationStatus::InvalidPacketType);
}
