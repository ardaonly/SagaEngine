/// @file AuthoritativeMovementCommandIntakeTests.cpp
/// @brief Unit tests for normalized authoritative movement command intake.

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCommandIntake.h"

#include "SagaEngine/Input/Commands/InputCommandSerializer.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace
{

using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::InputCommand;
using SagaEngine::Input::InputCommandSerializer;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::Networking::PacketType;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntake;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeConfig;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandIntakeDecision;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCommandPacketView;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementDecision;
using SagaEngine::ServerAuthority::Simulation::ClientId;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::Vector3;

constexpr ClientId kClient = 21;
constexpr EntityId kEntity = 909;

AuthoritativeMovementCommandIntakeConfig DefaultConfig()
{
    AuthoritativeMovementCommandIntakeConfig config;
    config.inputAdapter.movementCore.movementUnitsPerSecond = 6.0f;
    config.inputAdapter.movementCore.maxInputMagnitude = 1.0f;
    config.inputAdapter.movementCore.maxCommandsPerClientPerTick = 8;
    config.inputAdapter.movementCore.movementValidator.maxHorizontalSpeed = 12.0f;
    config.inputAdapter.movementCore.movementValidator.maxVerticalSpeed = 12.0f;
    config.inputAdapter.movementCore.movementValidator.maxAcceleration = 40.0f;
    config.inputAdapter.movementCore.movementValidator.teleportDistance = 20.0f;
    return config;
}

InputCommand NetworkCommand(std::uint32_t sequence,
                            float         moveX,
                            float         moveY,
                            std::uint32_t simulationTick = 1)
{
    auto command = MakeInputCommand(sequence, simulationTick, 1234);
    command.moveX = FixedFromFloat(moveX);
    command.moveY = FixedFromFloat(moveY);
    return command;
}

InputCommandSerializer::WireBuffer Serialize(const InputCommand& command)
{
    return InputCommandSerializer::Serialize(command);
}

AuthoritativeMovementCommandPacketView InputPacket(
    ClientId clientId,
    const std::uint8_t* payload,
    std::size_t payloadSize,
    std::uint64_t serverTick = 1,
    std::uint64_t recvTimeUnixMs = 5678)
{
    AuthoritativeMovementCommandPacketView packet;
    packet.clientId = clientId;
    packet.packetType = PacketType::InputCommand;
    packet.payload = payload;
    packet.payloadSize = payloadSize;
    packet.serverTick = serverTick;
    packet.recvTimeUnixMs = recvTimeUnixMs;
    return packet;
}

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

} // anonymous namespace

TEST(AuthoritativeMovementCommandIntakeTests, ValidInputPacketForwardsAndMutatesOnlyOnTick)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto bytes = Serialize(NetworkCommand(1, 0.0f, 1.0f));
    const auto result = intake.HandlePacket(
        InputPacket(kClient, bytes.data(), bytes.size()));

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::Forwarded);
    EXPECT_EQ(result.decodedSequence, 1u);
    ASSERT_TRUE(result.movementResult.has_value());
    EXPECT_EQ(result.movementResult->decision, AuthoritativeMovementDecision::Queued);
    EXPECT_FALSE(result.movementResult->mutated);
    EXPECT_FALSE(result.movementResult->dirty);

    auto beforeTick = intake.GetActorPosition(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto report = intake.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_TRUE(report.results[0].mutated);
    EXPECT_TRUE(report.results[0].dirty);

    auto afterTick = intake.GetActorPosition(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 0.0f, 0.0f, 6.0f);
}

TEST(AuthoritativeMovementCommandIntakeTests, NonInputPacketIsIgnored)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto bytes = Serialize(NetworkCommand(1, 1.0f, 0.0f));
    auto packet = InputPacket(kClient, bytes.data(), bytes.size());
    packet.packetType = PacketType::KeepAlive;

    const auto result = intake.HandlePacket(packet);

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::IgnoredNonInput);
    EXPECT_FALSE(result.movementResult.has_value());
    EXPECT_TRUE(intake.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCommandIntakeTests, MalformedPayloadSizeRejectsWithoutEnqueue)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const std::array<std::uint8_t, 3> malformed{1, 2, 3};
    const auto result = intake.HandlePacket(
        InputPacket(kClient, malformed.data(), malformed.size()));

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::MalformedInput);
    EXPECT_FALSE(result.movementResult.has_value());
    EXPECT_TRUE(intake.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCommandIntakeTests, NullInputPayloadRejectsWithoutEnqueue)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());

    const auto result = intake.HandlePacket(
        InputPacket(kClient, nullptr, InputCommandSerializer::kWireSize));

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::MalformedInput);
    EXPECT_FALSE(result.movementResult.has_value());
}

TEST(AuthoritativeMovementCommandIntakeTests, SerializerVersionMismatchIsMalformed)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    auto command = NetworkCommand(1, 1.0f, 0.0f);
    command.version = 255;
    const auto bytes = Serialize(command);

    const auto result = intake.HandlePacket(
        InputPacket(kClient, bytes.data(), bytes.size()));

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::MalformedInput);
    EXPECT_FALSE(result.movementResult.has_value());
    EXPECT_TRUE(intake.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCommandIntakeTests, UnknownClientReachesAdapterRejection)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    const auto bytes = Serialize(NetworkCommand(7, 1.0f, 0.0f));

    const auto result = intake.HandlePacket(
        InputPacket(kClient, bytes.data(), bytes.size()));

    EXPECT_EQ(result.decision, AuthoritativeMovementCommandIntakeDecision::Forwarded);
    EXPECT_EQ(result.decodedSequence, 7u);
    ASSERT_TRUE(result.movementResult.has_value());
    EXPECT_EQ(result.movementResult->decision,
              AuthoritativeMovementDecision::RejectUnknownClient);
    EXPECT_TRUE(intake.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCommandIntakeTests, DuplicateCommandReachesCoreQueueRejection)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto bytes = Serialize(NetworkCommand(1, 1.0f, 0.0f));

    const auto first = intake.HandlePacket(InputPacket(kClient, bytes.data(), bytes.size()));
    const auto duplicate = intake.HandlePacket(InputPacket(kClient, bytes.data(), bytes.size()));

    ASSERT_TRUE(first.movementResult.has_value());
    ASSERT_TRUE(duplicate.movementResult.has_value());
    EXPECT_EQ(first.movementResult->decision, AuthoritativeMovementDecision::Queued);
    EXPECT_EQ(duplicate.movementResult->decision, AuthoritativeMovementDecision::RejectQueue);

    const auto report = intake.Tick(1, 1.0f);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
}

TEST(AuthoritativeMovementCommandIntakeTests, DirtyEntitiesPassThroughAfterTick)
{
    AuthoritativeMovementCommandIntake intake(DefaultConfig());
    ASSERT_TRUE(intake.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto bytes = Serialize(NetworkCommand(1, 1.0f, 0.0f));
    ASSERT_EQ(intake.HandlePacket(
                  InputPacket(kClient, bytes.data(), bytes.size())).decision,
              AuthoritativeMovementCommandIntakeDecision::Forwarded);

    const auto report = intake.Tick(1, 1.0f);

    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds[0], kEntity);

    const auto dirty = intake.ConsumeDirtyEntities();
    ASSERT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], kEntity);
}
