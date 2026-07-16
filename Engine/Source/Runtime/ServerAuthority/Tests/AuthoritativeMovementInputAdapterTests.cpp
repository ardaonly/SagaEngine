/// @file AuthoritativeMovementInputAdapterTests.cpp
/// @brief Unit tests for the Engine-input to server movement-core adapter.

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementInputAdapter.h"

#include <gtest/gtest.h>

namespace
{

using SagaEngine::Input::FixedFromFloat;
using SagaEngine::Input::MakeInputCommand;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementDecision;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementInputAdapter;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementInputAdapterConfig;
using SagaEngine::ServerAuthority::Simulation::ClientId;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::Vector3;

constexpr ClientId kClient = 9;
constexpr ClientId kOtherClient = 10;
constexpr EntityId kEntity = 77;

AuthoritativeMovementInputAdapterConfig DefaultConfig()
{
    AuthoritativeMovementInputAdapterConfig config;
    config.movementCore.movementUnitsPerSecond = 6.0f;
    config.movementCore.maxInputMagnitude = 1.0f;
    config.movementCore.maxCommandsPerClientPerTick = 8;
    config.movementCore.movementValidator.maxHorizontalSpeed = 12.0f;
    config.movementCore.movementValidator.maxVerticalSpeed = 12.0f;
    config.movementCore.movementValidator.maxAcceleration = 40.0f;
    config.movementCore.movementValidator.teleportDistance = 20.0f;
    return config;
}

SagaEngine::Input::InputCommand NetworkCommand(
    std::uint32_t sequence,
    float         moveX,
    float         moveY,
    std::uint32_t simulationTick = 1)
{
    auto command = MakeInputCommand(sequence, simulationTick, 1234);
    command.moveX = FixedFromFloat(moveX);
    command.moveY = FixedFromFloat(moveY);
    return command;
}

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

} // anonymous namespace

TEST(AuthoritativeMovementInputAdapterTests, EngineInputConvertsToCoreMovement)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto enqueue = adapter.EnqueueNetworkInput(
        kClient, NetworkCommand(1, 0.0f, 1.0f), 1, 5678);

    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::Queued);
    EXPECT_FALSE(enqueue.mutated);
    EXPECT_FALSE(enqueue.dirty);

    auto beforeTick = adapter.GetActorPosition(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto report = adapter.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_TRUE(report.results[0].mutated);
    EXPECT_TRUE(report.results[0].dirty);

    auto afterTick = adapter.GetActorPosition(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 0.0f, 0.0f, 6.0f);
}

TEST(AuthoritativeMovementInputAdapterTests, WrongWireVersionRejectsBeforeCoreTick)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    auto command = NetworkCommand(1, 1.0f, 0.0f);
    command.version = 255;

    const auto enqueue = adapter.EnqueueNetworkInput(kClient, command, 1, 5678);

    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::RejectInput);
    EXPECT_FALSE(enqueue.mutated);
    EXPECT_FALSE(enqueue.dirty);
    EXPECT_TRUE(adapter.Tick(1, 1.0f).results.empty());

    auto position = adapter.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 0.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementInputAdapterTests, UnknownClientRejectsWithoutEnqueue)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());

    const auto enqueue = adapter.EnqueueNetworkInput(
        kClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678);

    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::RejectUnknownClient);
    EXPECT_FALSE(enqueue.mutated);
    EXPECT_FALSE(enqueue.dirty);
    EXPECT_TRUE(adapter.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementInputAdapterTests, SecondClientCannotClaimOwnedEntity)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    EXPECT_FALSE(adapter.RegisterActor(kOtherClient, kEntity, Vector3{1.0f, 0.0f, 0.0f}));

    const auto enqueue = adapter.EnqueueNetworkInput(
        kOtherClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678);

    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::RejectUnknownClient);
    EXPECT_TRUE(adapter.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementInputAdapterTests, OverMagnitudeInputRejectsThroughCore)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto enqueue = adapter.EnqueueNetworkInput(
        kClient, NetworkCommand(1, 2.0f, 0.0f), 1, 5678);

    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::RejectInput);
    EXPECT_TRUE(adapter.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementInputAdapterTests, DuplicateInputRejectsThroughCoreQueue)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    EXPECT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::Queued);
    EXPECT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::RejectQueue);

    const auto report = adapter.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
}

TEST(AuthoritativeMovementInputAdapterTests, QueuedInputsPreserveCoreSequenceOrdering)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    ASSERT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(3, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::Queued);
    ASSERT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::Queued);
    ASSERT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(2, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = adapter.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 3u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
    EXPECT_EQ(report.results[1].inputSequence, 2u);
    EXPECT_EQ(report.results[2].inputSequence, 3u);

    auto position = adapter.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 18.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementInputAdapterTests, DirtyEntitiesPassThroughFromCore)
{
    AuthoritativeMovementInputAdapter adapter(DefaultConfig());
    ASSERT_TRUE(adapter.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));
    ASSERT_EQ(adapter.EnqueueNetworkInput(
                  kClient, NetworkCommand(1, 1.0f, 0.0f), 1, 5678).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = adapter.Tick(1, 1.0f);

    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds[0], kEntity);

    const auto dirty = adapter.ConsumeDirtyEntities();
    ASSERT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], kEntity);
}
