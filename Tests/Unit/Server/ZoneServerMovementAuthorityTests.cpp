/// @file ZoneServerMovementAuthorityTests.cpp
/// @brief Socket-free tests for ZoneServer authoritative movement intake wiring.

#include "SagaServer/Networking/Server/ZoneServer.h"

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"

#include <gtest/gtest.h>

#include <cstring>
#include <cstdint>
#include <vector>

namespace
{

using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::InputCommand;
using SagaEngine::Input::InputCommandSerializer;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaEngine::Server::Simulation::ActorOwnershipResult;
using SagaEngine::Server::Simulation::AuthoritativeMovementCommandIntakeDecision;
using SagaEngine::Server::Simulation::AuthoritativeMovementDecision;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::Vector3;
using SagaServer::Networking::IZoneServerListener;
using SagaServer::Networking::NormalizedServerPacketView;
using SagaServer::Networking::ServerPacketNormalizationStatus;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

constexpr ClientId kClient = 31;
constexpr EntityId kEntity = 7001;

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
    config.zoneId = 12;
    config.zoneName = "movement-authority-test";
    config.enableDetailedPacketLog = false;
    return config;
}

InputCommand NetworkCommand(std::uint32_t sequence,
                            float moveX,
                            float moveY,
                            std::uint32_t simulationTick = 1)
{
    auto command = MakeInputCommand(sequence, simulationTick, 1234);
    command.moveX = FixedFromFloat(moveX);
    command.moveY = FixedFromFloat(moveY);
    return command;
}

std::vector<std::uint8_t> SerializeCommand(const InputCommand& command)
{
    const auto bytes = InputCommandSerializer::Serialize(command);
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

struct PacketObserver final : IZoneServerListener
{
    void OnInboundPacketNormalized(
        const NormalizedServerPacketView& packet) override
    {
        ++callCount;
        packetType = packet.packetType;
    }

    int callCount{0};
    PacketType packetType{PacketType::Invalid};
};

} // namespace

TEST(ZoneServerMovementAuthorityTests, RegisteredActorInputMutatesOnlyAfterTick)
{
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    const auto payload = SerializeCommand(NetworkCommand(1, 0.0f, 1.0f));
    const auto frame = MakeFrame(PacketType::InputCommand, payload);

    const auto status = server.ProcessRawInboundPacketForTesting(
        kClient,
        frame.data(),
        frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::Success);

    const auto intake = server.GetLastMovementIntakeResultForTesting();
    ASSERT_TRUE(intake.has_value());
    EXPECT_EQ(intake->decision, AuthoritativeMovementCommandIntakeDecision::Forwarded);
    ASSERT_TRUE(intake->movementResult.has_value());
    EXPECT_EQ(intake->movementResult->decision, AuthoritativeMovementDecision::Queued);

    const auto beforeTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto report = server.TickMovementForTesting(1, 1.0);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_TRUE(report.results[0].mutated);
    EXPECT_TRUE(report.results[0].dirty);

    const auto afterTick = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 0.0f, 0.0f, 6.0f);
}

TEST(ZoneServerMovementAuthorityTests, UnknownClientInputRejects)
{
    ZoneServer server(TestConfig());

    const auto payload = SerializeCommand(NetworkCommand(7, 1.0f, 0.0f));
    const auto frame = MakeFrame(PacketType::InputCommand, payload);

    const auto status = server.ProcessRawInboundPacketForTesting(
        kClient,
        frame.data(),
        frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::Success);

    const auto intake = server.GetLastMovementIntakeResultForTesting();
    ASSERT_TRUE(intake.has_value());
    EXPECT_EQ(intake->decision, AuthoritativeMovementCommandIntakeDecision::Forwarded);
    ASSERT_TRUE(intake->movementResult.has_value());
    EXPECT_EQ(intake->movementResult->decision,
              AuthoritativeMovementDecision::RejectUnknownClient);
    EXPECT_TRUE(server.TickMovementForTesting(1, 1.0).results.empty());
}

TEST(ZoneServerMovementAuthorityTests, MalformedFrameRejectsBeforeIntake)
{
    ZoneServer server(TestConfig());

    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[0] ^= 0xFF;

    const auto status = server.ProcessRawInboundPacketForTesting(
        kClient,
        frame.data(),
        frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::BadMagic);
    EXPECT_FALSE(server.GetLastMovementIntakeResultForTesting().has_value());
    EXPECT_EQ(server.GetStats().totalPacketsRejected.load(), 1u);
}

TEST(ZoneServerMovementAuthorityTests, NonInputFrameNormalizesButDoesNotEnqueue)
{
    ZoneServer server(TestConfig());
    PacketObserver observer;
    server.AddListener(&observer);
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{1.0f, 2.0f, 3.0f}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(PacketType::KeepAlive, {0x01, 0x02});

    const auto status = server.ProcessRawInboundPacketForTesting(
        kClient,
        frame.data(),
        frame.size());

    EXPECT_EQ(status, ServerPacketNormalizationStatus::Success);
    EXPECT_EQ(observer.callCount, 1);
    EXPECT_EQ(observer.packetType, PacketType::KeepAlive);
    EXPECT_FALSE(server.GetLastMovementIntakeResultForTesting().has_value());
    EXPECT_TRUE(server.TickMovementForTesting(1, 1.0).results.empty());

    const auto position = server.GetControlledActorPositionForTesting(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 1.0f, 2.0f, 3.0f);
}
