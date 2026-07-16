/// @file ActorOwnershipRegistryTests.cpp
/// @brief Unit tests for server actor ownership mapping.

#include "SagaEngine/ServerAuthority/Simulation/ActorOwnershipRegistry.h"

#include <gtest/gtest.h>

namespace
{

using SagaEngine::ServerAuthority::Simulation::ActorOwnershipRegistry;
using SagaEngine::ServerAuthority::Simulation::ActorOwnershipResult;
using SagaEngine::ServerAuthority::Simulation::ClientId;
using SagaEngine::ServerAuthority::Simulation::EntityId;
using SagaEngine::ServerAuthority::Simulation::Vector3;

constexpr ClientId kClient = 11;
constexpr ClientId kOtherClient = 12;
constexpr EntityId kEntity = 101;
constexpr EntityId kOtherEntity = 202;

void ExpectPosition(const Vector3& position, float x, float y, float z)
{
    EXPECT_FLOAT_EQ(position.x, x);
    EXPECT_FLOAT_EQ(position.y, y);
    EXPECT_FLOAT_EQ(position.z, z);
}

} // anonymous namespace

TEST(ActorOwnershipRegistryTests, RegisterValidOwnershipAndLookupBothDirections)
{
    ActorOwnershipRegistry registry;

    EXPECT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{1.0f, 2.0f, 3.0f}),
              ActorOwnershipResult::Registered);

    EXPECT_EQ(registry.Count(), 1u);
    EXPECT_TRUE(registry.Owns(kClient, kEntity));

    const auto byClient = registry.FindByClient(kClient);
    ASSERT_TRUE(byClient.has_value());
    EXPECT_EQ(byClient->clientId, kClient);
    EXPECT_EQ(byClient->entityId, kEntity);
    ExpectPosition(byClient->initialPosition, 1.0f, 2.0f, 3.0f);

    const auto byEntity = registry.FindByEntity(kEntity);
    ASSERT_TRUE(byEntity.has_value());
    EXPECT_EQ(byEntity->clientId, kClient);
    EXPECT_EQ(byEntity->entityId, kEntity);
}

TEST(ActorOwnershipRegistryTests, RejectsInvalidClient)
{
    ActorOwnershipRegistry registry;

    EXPECT_EQ(registry.RegisterOwnership(0, kEntity, Vector3{}),
              ActorOwnershipResult::InvalidClient);
    EXPECT_EQ(registry.UnregisterClient(0), ActorOwnershipResult::InvalidClient);
    EXPECT_EQ(registry.Count(), 0u);
}

TEST(ActorOwnershipRegistryTests, RejectsInvalidEntity)
{
    ActorOwnershipRegistry registry;

    EXPECT_EQ(registry.RegisterOwnership(kClient, 0, Vector3{}),
              ActorOwnershipResult::InvalidEntity);
    EXPECT_EQ(registry.UnregisterEntity(0), ActorOwnershipResult::InvalidEntity);
    EXPECT_EQ(registry.Count(), 0u);
}

TEST(ActorOwnershipRegistryTests, DuplicateSamePairIsIdempotentAndPreservesMapping)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{1.0f, 2.0f, 3.0f}),
              ActorOwnershipResult::Registered);
    EXPECT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{4.0f, 5.0f, 6.0f}),
              ActorOwnershipResult::AlreadyRegistered);

    EXPECT_EQ(registry.Count(), 1u);

    const auto actor = registry.FindByClient(kClient);
    ASSERT_TRUE(actor.has_value());
    ExpectPosition(actor->initialPosition, 1.0f, 2.0f, 3.0f);
}

TEST(ActorOwnershipRegistryTests, SameClientCannotOwnDifferentEntityWithoutUnregister)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);

    EXPECT_EQ(registry.RegisterOwnership(kClient, kOtherEntity, Vector3{}),
              ActorOwnershipResult::ClientAlreadyOwnsDifferentEntity);
    EXPECT_TRUE(registry.Owns(kClient, kEntity));
    EXPECT_FALSE(registry.Owns(kClient, kOtherEntity));
    EXPECT_EQ(registry.Count(), 1u);
}

TEST(ActorOwnershipRegistryTests, SameEntityCannotBeOwnedByDifferentClientWithoutUnregister)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);

    EXPECT_EQ(registry.RegisterOwnership(kOtherClient, kEntity, Vector3{}),
              ActorOwnershipResult::EntityAlreadyOwnedByDifferentClient);
    EXPECT_TRUE(registry.Owns(kClient, kEntity));
    EXPECT_FALSE(registry.Owns(kOtherClient, kEntity));
    EXPECT_EQ(registry.Count(), 1u);
}

TEST(ActorOwnershipRegistryTests, UnregisterClientRemovesBothDirections)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);

    EXPECT_EQ(registry.UnregisterClient(kClient), ActorOwnershipResult::Unregistered);
    EXPECT_EQ(registry.Count(), 0u);
    EXPECT_FALSE(registry.FindByClient(kClient).has_value());
    EXPECT_FALSE(registry.FindByEntity(kEntity).has_value());
    EXPECT_FALSE(registry.Owns(kClient, kEntity));
    EXPECT_EQ(registry.UnregisterClient(kClient), ActorOwnershipResult::NotFound);
}

TEST(ActorOwnershipRegistryTests, UnregisterEntityRemovesBothDirections)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);

    EXPECT_EQ(registry.UnregisterEntity(kEntity), ActorOwnershipResult::Unregistered);
    EXPECT_EQ(registry.Count(), 0u);
    EXPECT_FALSE(registry.FindByClient(kClient).has_value());
    EXPECT_FALSE(registry.FindByEntity(kEntity).has_value());
    EXPECT_FALSE(registry.Owns(kClient, kEntity));
    EXPECT_EQ(registry.UnregisterEntity(kEntity), ActorOwnershipResult::NotFound);
}

TEST(ActorOwnershipRegistryTests, ClearRemovesAllMappings)
{
    ActorOwnershipRegistry registry;

    ASSERT_EQ(registry.RegisterOwnership(kClient, kEntity, Vector3{}),
              ActorOwnershipResult::Registered);
    ASSERT_EQ(registry.RegisterOwnership(kOtherClient, kOtherEntity, Vector3{}),
              ActorOwnershipResult::Registered);

    registry.Clear();

    EXPECT_EQ(registry.Count(), 0u);
    EXPECT_FALSE(registry.FindByClient(kClient).has_value());
    EXPECT_FALSE(registry.FindByClient(kOtherClient).has_value());
    EXPECT_FALSE(registry.FindByEntity(kEntity).has_value());
    EXPECT_FALSE(registry.FindByEntity(kOtherEntity).has_value());
}
