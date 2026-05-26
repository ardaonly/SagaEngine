/// @file MovementDirtyReplicationBridgeTests.cpp
/// @brief Deterministic tests for authoritative movement dirty replication intent.

#include "SagaServer/Networking/Replication/MovementDirtyReplicationBridge.h"
#include "SagaServer/Networking/Server/ZoneServer.h"

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace
{

using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::InputCommand;
using SagaEngine::Input::InputCommandSerializer;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::Networking::ClientId;
using SagaEngine::Networking::PacketType;
using SagaEngine::Networking::Replication::MovementDirtyReplicationBridge;
using SagaEngine::Networking::Replication::MovementDirtyReplicationIntent;
using SagaEngine::Server::Simulation::ActorOwnershipResult;
using SagaEngine::Server::Simulation::AuthoritativeMovementCommandIntakeDecision;
using SagaEngine::Server::Simulation::AuthoritativeMovementDecision;
using SagaEngine::Server::Simulation::AuthoritativeMovementTickReport;
using SagaEngine::Server::Simulation::EntityId;
using SagaEngine::Server::Simulation::Vector3;
using SagaServer::Networking::ServerPacketNormalizationStatus;
using SagaServer::Networking::ZoneServer;
using SagaServer::Networking::ZoneServerConfig;

constexpr ClientId kClient = 41;
constexpr EntityId kEntity = 8101;

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
    config.zoneId = 13;
    config.zoneName = "movement-dirty-replication-test";
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

void QueueInputFrame(ZoneServer& server, ClientId clientId, const InputCommand& command)
{
    const auto payload = SerializeCommand(command);
    const auto frame = MakeFrame(PacketType::InputCommand, payload);

    ASSERT_EQ(server.ProcessRawInboundPacketForTesting(
                  clientId,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);
}

} // namespace

TEST(MovementDirtyReplicationBridgeTests, BridgeDeduplicatesPendingEntityAndKeepsLatestTick)
{
    MovementDirtyReplicationBridge bridge;

    AuthoritativeMovementTickReport first;
    first.dirtyEntityIds = {kEntity, kEntity};
    bridge.RecordMovementTick(first, 10);

    AuthoritativeMovementTickReport second;
    second.dirtyEntityIds = {kEntity};
    bridge.RecordMovementTick(second, 12);

    EXPECT_EQ(bridge.PendingCount(), 1u);

    const std::vector<MovementDirtyReplicationIntent> intents =
        bridge.ConsumeDirtyMovementIntents();
    ASSERT_EQ(intents.size(), 1u);
    EXPECT_EQ(intents[0].entityId, kEntity);
    EXPECT_EQ(intents[0].serverTick, 12u);
    EXPECT_EQ(bridge.PendingCount(), 0u);
}

TEST(MovementDirtyReplicationBridgeTests, AcceptedRegisteredMovementProducesIntentAfterTick)
{
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    QueueInputFrame(server, kClient, NetworkCommand(1, 0.0f, 1.0f));
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);

    const auto report = server.TickMovementForTesting(20, 1.0);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_TRUE(report.results[0].dirty);
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 1u);

    const std::vector<MovementDirtyReplicationIntent> intents =
        server.ConsumeMovementDirtyReplicationIntentsForTesting();
    ASSERT_EQ(intents.size(), 1u);
    EXPECT_EQ(intents[0].entityId, kEntity);
    EXPECT_EQ(intents[0].serverTick, 20u);
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);
}

TEST(MovementDirtyReplicationBridgeTests, UnknownClientRejectsWithoutDirtyIntent)
{
    ZoneServer server(TestConfig());

    QueueInputFrame(server, kClient, NetworkCommand(7, 1.0f, 0.0f));

    const auto intake = server.GetLastMovementIntakeResultForTesting();
    ASSERT_TRUE(intake.has_value());
    EXPECT_EQ(intake->decision, AuthoritativeMovementCommandIntakeDecision::Forwarded);
    ASSERT_TRUE(intake->movementResult.has_value());
    EXPECT_EQ(intake->movementResult->decision,
              AuthoritativeMovementDecision::RejectUnknownClient);

    EXPECT_TRUE(server.TickMovementForTesting(21, 1.0).results.empty());
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);
    EXPECT_TRUE(server.ConsumeMovementDirtyReplicationIntentsForTesting().empty());
}

TEST(MovementDirtyReplicationBridgeTests, MalformedFrameRejectsBeforeDirtyIntent)
{
    ZoneServer server(TestConfig());

    auto frame = MakeFrame(PacketType::InputCommand, {0x01});
    frame[0] ^= 0xFF;

    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::BadMagic);

    EXPECT_FALSE(server.GetLastMovementIntakeResultForTesting().has_value());
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);
    EXPECT_TRUE(server.ConsumeMovementDirtyReplicationIntentsForTesting().empty());
}

TEST(MovementDirtyReplicationBridgeTests, NonInputFrameNormalizesWithoutDirtyIntent)
{
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    const auto frame = MakeFrame(PacketType::KeepAlive, {0x01, 0x02});
    EXPECT_EQ(server.ProcessRawInboundPacketForTesting(
                  kClient,
                  frame.data(),
                  frame.size()),
              ServerPacketNormalizationStatus::Success);

    EXPECT_FALSE(server.GetLastMovementIntakeResultForTesting().has_value());
    EXPECT_TRUE(server.TickMovementForTesting(22, 1.0).results.empty());
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);
}

TEST(MovementDirtyReplicationBridgeTests, NonMutatingAcceptedInputDoesNotProduceDirtyIntent)
{
    ZoneServer server(TestConfig());
    ASSERT_EQ(server.RegisterControlledActor(
                  kClient,
                  kEntity,
                  Vector3{0.0f, 0.0f, 0.0f}),
              ActorOwnershipResult::Registered);

    QueueInputFrame(server, kClient, NetworkCommand(9, 0.0f, 0.0f));

    const auto report = server.TickMovementForTesting(23, 1.0);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_FALSE(report.results[0].dirty);
    EXPECT_EQ(server.GetPendingMovementDirtyReplicationIntentCountForTesting(), 0u);
}
