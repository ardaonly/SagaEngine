/// @file AuthoritativeMovementCoreTests.cpp
/// @brief Unit tests for the server-owned authoritative movement core.

#include "SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCore;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementCoreConfig;
using SagaEngine::ServerAuthority::Simulation::AuthoritativeMovementDecision;
using SagaEngine::ServerAuthority::Simulation::ClientId;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::InputCommand;
using SagaEngine::ServerAuthority::Simulation::MovementDecision;
using SagaEngine::ServerAuthority::Simulation::Vector3;

constexpr ClientId kClient = 7;
constexpr EntityId kEntity = 42;

InputCommand MoveCommand(std::uint64_t sequence,
                         float         x = 1.0f,
                         float         y = 0.0f,
                         float         z = 0.0f)
{
    InputCommand command;
    command.sequence = sequence;
    command.clientTick = 1;
    command.serverRecvTick = 1;
    command.moveX = x;
    command.moveY = y;
    command.moveZ = z;
    return command;
}

AuthoritativeMovementCoreConfig DefaultConfig()
{
    AuthoritativeMovementCoreConfig config;
    config.movementUnitsPerSecond = 6.0f;
    config.maxInputMagnitude = 1.0f;
    config.maxCommandsPerClientPerTick = 8;
    config.movementValidator.maxHorizontalSpeed = 12.0f;
    config.movementValidator.maxVerticalSpeed = 12.0f;
    config.movementValidator.maxAcceleration = 40.0f;
    config.movementValidator.teleportDistance = 20.0f;
    return config;
}

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

} // anonymous namespace

TEST(AuthoritativeMovementCoreTests, OwnedInputMutatesOnlyDuringTick)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto enqueue = core.EnqueueInput(kClient, kEntity, MoveCommand(1));
    EXPECT_EQ(enqueue.decision, AuthoritativeMovementDecision::Queued);
    EXPECT_FALSE(enqueue.mutated);
    EXPECT_FALSE(enqueue.dirty);

    auto beforeTick = core.GetActorPosition(kEntity);
    ASSERT_TRUE(beforeTick.has_value());
    ExpectPosition(*beforeTick, 0.0f, 0.0f, 0.0f);

    const auto report = core.Tick(1, 1.0f);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Accepted);
    EXPECT_EQ(report.results[0].validatorDecision, MovementDecision::Accept);
    EXPECT_TRUE(report.results[0].mutated);
    EXPECT_TRUE(report.results[0].dirty);

    auto afterTick = core.GetActorPosition(kEntity);
    ASSERT_TRUE(afterTick.has_value());
    ExpectPosition(*afterTick, 6.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementCoreTests, UnknownClientInputIsRejected)
{
    AuthoritativeMovementCore core(DefaultConfig());

    const auto result = core.EnqueueInput(kClient, kEntity, MoveCommand(1));

    EXPECT_EQ(result.decision, AuthoritativeMovementDecision::RejectUnknownClient);
    EXPECT_FALSE(result.mutated);
    EXPECT_FALSE(result.dirty);
    EXPECT_TRUE(core.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCoreTests, MismatchedOwnershipIsRejected)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    const auto result = core.EnqueueInput(kClient, static_cast<EntityId>(99), MoveCommand(1));

    EXPECT_EQ(result.decision, AuthoritativeMovementDecision::RejectOwnership);
    EXPECT_FALSE(result.mutated);
    EXPECT_TRUE(core.Tick(1, 1.0f).results.empty());

    auto position = core.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 0.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementCoreTests, OverMagnitudeAndNonFiniteInputAreRejected)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    EXPECT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1, 2.0f)).decision,
              AuthoritativeMovementDecision::RejectInput);

    auto nonFinite = MoveCommand(2);
    nonFinite.moveX = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(core.EnqueueInput(kClient, kEntity, nonFinite).decision,
              AuthoritativeMovementDecision::RejectInput);

    EXPECT_TRUE(core.Tick(1, 1.0f).results.empty());
}

TEST(AuthoritativeMovementCoreTests, ValidatorRejectionDoesNotMutateOrMarkDirty)
{
    auto config = DefaultConfig();
    config.movementUnitsPerSecond = 10.0f;
    config.movementValidator.teleportDistance = 1.0f;

    AuthoritativeMovementCore core(config);
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));
    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = core.Tick(1, 1.0f);
    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::RejectedByValidator);
    EXPECT_EQ(report.results[0].validatorDecision, MovementDecision::RejectTeleport);
    EXPECT_FALSE(report.results[0].mutated);
    EXPECT_FALSE(report.results[0].dirty);
    EXPECT_TRUE(report.dirtyEntityIds.empty());
    EXPECT_TRUE(core.ConsumeDirtyEntities().empty());

    auto position = core.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 0.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementCoreTests, AcceptedMovementMarksEntityDirty)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));
    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = core.Tick(1, 1.0f);

    ASSERT_EQ(report.dirtyEntityIds.size(), 1u);
    EXPECT_EQ(report.dirtyEntityIds[0], kEntity);

    const auto dirty = core.ConsumeDirtyEntities();
    ASSERT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], kEntity);
    EXPECT_TRUE(core.ConsumeDirtyEntities().empty());
}

TEST(AuthoritativeMovementCoreTests, ClampedMovementMutatesAndMarksDirty)
{
    auto config = DefaultConfig();
    config.movementValidator.maxHorizontalSpeed = 1.0f;
    config.movementValidator.maxAcceleration = 100.0f;
    config.movementValidator.teleportDistance = 20.0f;
    config.movementValidator.toleranceBudget = 0.0f;

    AuthoritativeMovementCore core(config);
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));
    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = core.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].decision, AuthoritativeMovementDecision::Clamped);
    EXPECT_EQ(report.results[0].validatorDecision, MovementDecision::Clamp);
    EXPECT_TRUE(report.results[0].mutated);
    EXPECT_TRUE(report.results[0].dirty);

    auto position = core.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 1.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementCoreTests, QueuedInputsDrainInSequenceOrder)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(3)).decision,
              AuthoritativeMovementDecision::Queued);
    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::Queued);
    ASSERT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(2)).decision,
              AuthoritativeMovementDecision::Queued);

    const auto report = core.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 3u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
    EXPECT_EQ(report.results[1].inputSequence, 2u);
    EXPECT_EQ(report.results[2].inputSequence, 3u);

    auto position = core.GetActorPosition(kEntity);
    ASSERT_TRUE(position.has_value());
    ExpectPosition(*position, 18.0f, 0.0f, 0.0f);
}

TEST(AuthoritativeMovementCoreTests, DuplicateInputIsRejectedByQueue)
{
    AuthoritativeMovementCore core(DefaultConfig());
    ASSERT_TRUE(core.RegisterActor(kClient, kEntity, Vector3{0.0f, 0.0f, 0.0f}));

    EXPECT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::Queued);
    EXPECT_EQ(core.EnqueueInput(kClient, kEntity, MoveCommand(1)).decision,
              AuthoritativeMovementDecision::RejectQueue);

    const auto report = core.Tick(1, 1.0f);

    ASSERT_EQ(report.results.size(), 1u);
    EXPECT_EQ(report.results[0].inputSequence, 1u);
}
