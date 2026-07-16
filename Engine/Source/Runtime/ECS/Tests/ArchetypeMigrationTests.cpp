// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/ECS/ArchetypeMigration.h"

#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

namespace SagaEngine::ECS {
namespace {

TEST(ArchetypeTests, ComponentSetsAreCanonicalAndReuseOneArchetype)
{
    ArchetypeManager manager;
    auto& first = manager.GetOrCreateArchetype({3, 1, 3, 2});
    auto& second = manager.GetOrCreateArchetype({2, 1, 3});

    EXPECT_EQ(&first, &second);
    EXPECT_EQ(first.GetComponentTypes(), (std::vector<ComponentTypeId>{1, 2, 3}));
    EXPECT_EQ(manager.GetAllArchetypes().size(), 1u);
}

TEST(ArchetypeMigrationTests, ImmediateMigrationMovesEntityAndNotifiesObserver)
{
    ArchetypeManager manager;
    auto& source = manager.GetOrCreateArchetype({10});
    constexpr EntityId entity = 77;
    source.AddEntity(entity);

    std::unordered_map<EntityId, Archetype*> locations{{entity, &source}};
    ArchetypeMigrator migrator(&manager, &locations);
    int notifications = 0;
    migrator.SetOnMigration([&](EntityId migrated, const Archetype& oldType,
                                const Archetype& newType) {
        EXPECT_EQ(migrated, entity);
        EXPECT_TRUE(oldType.HasComponent(10));
        EXPECT_TRUE(newType.HasComponent(20));
        ++notifications;
    });

    EXPECT_EQ(migrator.AddComponent(entity, 20), MigrationResult::Success);
    ASSERT_NE(locations.at(entity), nullptr);
    EXPECT_TRUE(locations.at(entity)->HasComponent(10));
    EXPECT_TRUE(locations.at(entity)->HasComponent(20));
    EXPECT_EQ(source.GetEntityCount(), 0u);
    EXPECT_EQ(locations.at(entity)->GetEntities(), (std::vector<EntityId>{entity}));
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(migrator.AddComponent(entity, 20), MigrationResult::AlreadyHasComponent);
}

TEST(ArchetypeMigrationTests, DeferredQueueReportsOnlySuccessfulMigrations)
{
    ArchetypeManager manager;
    auto& source = manager.GetOrCreateArchetype({1, 2});
    constexpr EntityId entity = 8;
    source.AddEntity(entity);
    std::unordered_map<EntityId, Archetype*> locations{{entity, &source}};
    ArchetypeMigrator migrator(&manager, &locations);

    migrator.EnqueueRemove(entity, 2);
    migrator.EnqueueRemove(entity, 99);
    migrator.EnqueueAdd(999, 4);

    EXPECT_EQ(migrator.DeferredCount(), 3u);
    EXPECT_EQ(migrator.FlushDeferred(), 1u);
    EXPECT_EQ(migrator.DeferredCount(), 0u);
    EXPECT_TRUE(locations.at(entity)->HasComponent(1));
    EXPECT_FALSE(locations.at(entity)->HasComponent(2));
}

} // namespace
} // namespace SagaEngine::ECS
